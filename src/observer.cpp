#include "nv_research/observer.hpp"
#include "nv_research/slot_layout.hpp"

#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace nv_research {

namespace {

struct s_handle {
    HANDLE m_h = nullptr;
    s_handle() = default;
    explicit s_handle(HANDLE h) : m_h(h) {}
    s_handle(const s_handle&) = delete;
    s_handle& operator=(const s_handle&) = delete;
    ~s_handle() { if (m_h && m_h != INVALID_HANDLE_VALUE) CloseHandle(m_h); }
    explicit operator bool() const { return m_h && m_h != INVALID_HANDLE_VALUE; }
};

struct s_view {
    void* m_p = nullptr;
    explicit s_view(void* p) : m_p(p) {}
    s_view(const s_view&) = delete;
    s_view& operator=(const s_view&) = delete;
    ~s_view() { if (m_p) UnmapViewOfFile(m_p); }
};

template <typename t>
t read_at(const std::byte* bytes, std::size_t offset) {
    t value{};
    std::memcpy(&value, bytes + offset, sizeof(t));
    return value;
}

std::uint64_t filetime_ticks(const FILETIME& ft) {
    return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

s_error make_error(const char* stage, DWORD code) {
    s_error e;
    e.m_stage = stage;
    e.m_code = code;
    return e;
}

}

const char* status_name(e_slot_status status) noexcept {
    switch (status) {
    case e_slot_status::ok: return "ok";
    case e_slot_status::absent: return "absent";
    case e_slot_status::access_denied: return "access_denied";
    case e_slot_status::open_error: return "open_error";
    case e_slot_status::map_error: return "map_error";
    case e_slot_status::insufficient_range: return "insufficient_range";
    case e_slot_status::unknown_version: return "unknown_version";
    case e_slot_status::unstable: return "unstable";
    }
    return "unknown";
}

s_slot_sample decode_slot(const std::byte* bytes, std::size_t size, unsigned index) {
    s_slot_sample s;
    s.m_index = index;
    if (size < sizeof(std::uint32_t)) {
        s.m_status = e_slot_status::insufficient_range;
        s.m_error = make_error("decode", 0);
        return s;
    }
    s.m_version = read_at<std::uint32_t>(bytes, layout::g_off_version);
    if (s.m_version != layout::g_expected_version) {
        s.m_status = e_slot_status::unknown_version;
        return s;
    }
    if (size < layout::g_logical_size) {
        s.m_status = e_slot_status::insufficient_range;
        s.m_error = make_error("decode", 0);
        return s;
    }
    s.m_has_fields = true;
    s.m_transport = read_at<std::uint32_t>(bytes, layout::g_off_transport);
    s.m_flags = read_at<std::uint32_t>(bytes, layout::g_off_flags);
    s.m_update_counter = read_at<std::uint32_t>(bytes, layout::g_off_update_counter);
    s.m_render_counter = read_at<std::uint32_t>(bytes, layout::g_off_render_counter);
    s.m_generation = read_at<std::uint64_t>(bytes, layout::g_off_generation);
    s.m_width = read_at<std::uint32_t>(bytes, layout::g_off_width);
    s.m_height = read_at<std::uint32_t>(bytes, layout::g_off_height);
    s.m_alpha = read_at<float>(bytes, layout::g_off_alpha);
    s.m_status = e_slot_status::ok;
    return s;
}

bool equivalent(const s_slot_sample& a, const s_slot_sample& b) noexcept {
    return a.m_status == b.m_status && a.m_error.m_code == b.m_error.m_code &&
           a.m_version == b.m_version && a.m_has_fields == b.m_has_fields &&
           a.m_transport == b.m_transport && a.m_flags == b.m_flags &&
           a.m_update_counter == b.m_update_counter &&
           a.m_render_counter == b.m_render_counter &&
           a.m_generation == b.m_generation && a.m_width == b.m_width &&
           a.m_height == b.m_height &&
           std::memcmp(&a.m_alpha, &b.m_alpha, sizeof(float)) == 0;
}

bool equivalent(const s_process_context& a, const s_process_context& b) noexcept {
    if (a.m_processes.size() != b.m_processes.size()) return false;
    for (std::size_t i = 0; i < a.m_processes.size(); ++i) {
        const auto& l = a.m_processes[i];
        const auto& r = b.m_processes[i];
        if (l.m_pid != r.m_pid || l.m_creation_time != r.m_creation_time ||
            l.m_session_id != r.m_session_id || l.m_path != r.m_path)
            return false;
    }
    return true;
}

s_slot_sample read_slot(const std::wstring& name, unsigned index) {
    s_slot_sample result;
    result.m_index = index;

    for (int attempt = 0; attempt < 3; ++attempt) {
        s_handle mapping{OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str())};
        if (!mapping) {
            const DWORD code = GetLastError();
            s_slot_sample s;
            s.m_index = index;
            s.m_error = make_error("open_file_mapping", code);
            if (code == ERROR_FILE_NOT_FOUND) s.m_status = e_slot_status::absent;
            else if (code == ERROR_ACCESS_DENIED) s.m_status = e_slot_status::access_denied;
            else s.m_status = e_slot_status::open_error;
            return s;
        }
        void* view_ptr = MapViewOfFile(mapping.m_h, FILE_MAP_READ, 0, 0, 0);
        if (!view_ptr) {
            s_slot_sample s;
            s.m_index = index;
            s.m_status = e_slot_status::map_error;
            s.m_error = make_error("map_view_of_file", GetLastError());
            return s;
        }
        s_view view{view_ptr};

        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(view_ptr, &mbi, sizeof(mbi)) == 0) {
            s_slot_sample s;
            s.m_index = index;
            s.m_status = e_slot_status::map_error;
            s.m_error = make_error("virtual_query", GetLastError());
            return s;
        }
        const std::size_t readable = mbi.RegionSize;
        const std::size_t copy_size = std::min<std::size_t>(readable, layout::g_logical_size);
        std::vector<std::byte> copy(copy_size);
        std::memcpy(copy.data(), view_ptr, copy_size);

        s_slot_sample first = decode_slot(copy.data(), copy_size, index);

        std::vector<std::byte> copy2(copy_size);
        std::memcpy(copy2.data(), view_ptr, copy_size);
        s_slot_sample second = decode_slot(copy2.data(), copy_size, index);

        if (equivalent(first, second)) return first;
        result = second;
    }
    result.m_status = e_slot_status::unstable;
    return result;
}

std::array<s_slot_sample, 8> scan_slots() {
    std::array<s_slot_sample, 8> rows{};
    for (unsigned i = 0; i < layout::g_slot_count; ++i)
        rows[i] = read_slot(layout::section_name(i), i);
    return rows;
}

std::uint32_t current_session_id() {
    DWORD id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &id)) return 0xFFFFFFFFu;
    return id;
}

namespace {

s_process describe_process(const PROCESSENTRY32W& pe) {
    s_process p;
    p.m_pid = pe.th32ProcessID;
    p.m_name = pe.szExeFile;

    DWORD session = 0;
    if (ProcessIdToSessionId(pe.th32ProcessID, &session)) p.m_session_id = session;
    else p.m_errors.push_back(make_error("process_id_to_session_id", GetLastError()));

    s_handle h{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID)};
    if (!h) {
        p.m_errors.push_back(make_error("open_process", GetLastError()));
        return p;
    }
    FILETIME creation{}, exit_time{}, kernel{}, user{};
    if (GetProcessTimes(h.m_h, &creation, &exit_time, &kernel, &user))
        p.m_creation_time = filetime_ticks(creation);
    else
        p.m_errors.push_back(make_error("get_process_times", GetLastError()));

    std::vector<wchar_t> path(32768);
    DWORD len = static_cast<DWORD>(path.size());
    if (QueryFullProcessImageNameW(h.m_h, 0, path.data(), &len))
        p.m_path.assign(path.data(), len);
    else
        p.m_errors.push_back(make_error("query_full_process_image_name", GetLastError()));
    return p;
}

}

s_process_context query_processes(std::optional<std::uint32_t> pid,
                                  const std::wstring& image_name) {
    s_process_context ctx;
    s_handle snap{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snap) {
        ctx.m_errors.push_back(make_error("create_toolhelp32_snapshot", GetLastError()));
        return ctx;
    }
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (!Process32FirstW(snap.m_h, &pe)) {
        ctx.m_errors.push_back(make_error("process32_first", GetLastError()));
        return ctx;
    }
    do {
        if (pid && pe.th32ProcessID != *pid) continue;
        if (!image_name.empty() && _wcsicmp(pe.szExeFile, image_name.c_str()) != 0) continue;
        ctx.m_processes.push_back(describe_process(pe));
    } while (Process32NextW(snap.m_h, &pe));
    return ctx;
}

s_security query_security(const std::wstring& name, bool is_mutex) {
    s_security sec;
    sec.m_name = name;
    sec.m_is_mutex = is_mutex;

    HANDLE raw = nullptr;
    if (is_mutex) {
        raw = OpenMutexW(READ_CONTROL, FALSE, name.c_str());
    } else {
        raw = OpenFileMappingW(READ_CONTROL, FALSE, name.c_str());
    }
    s_handle h{raw};
    if (!h) {
        sec.m_error = make_error(is_mutex ? "open_mutex" : "open_file_mapping", GetLastError());
        return sec;
    }

    PSID owner = nullptr;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const DWORD rc = GetSecurityInfo(h.m_h, SE_KERNEL_OBJECT,
                                     OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                                     &owner, nullptr, &dacl, nullptr, &descriptor);
    if (rc != ERROR_SUCCESS) {
        sec.m_error = make_error("get_security_info", rc);
        return sec;
    }
    LPWSTR sid_string = nullptr;
    if (owner && ConvertSidToStringSidW(owner, &sid_string)) {
        sec.m_owner_sid = sid_string;
        LocalFree(sid_string);
    }
    LPWSTR sddl = nullptr;
    if (ConvertSecurityDescriptorToStringSecurityDescriptorW(
            descriptor, SDDL_REVISION_1,
            OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &sddl, nullptr)) {
        sec.m_sddl = sddl;
        LocalFree(sddl);
    } else {
        sec.m_error = make_error("convert_security_descriptor", GetLastError());
    }
    LocalFree(descriptor);
    sec.m_available = !sec.m_sddl.empty();
    return sec;
}

}
