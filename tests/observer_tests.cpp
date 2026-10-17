#include "nv_research/observer.hpp"
#include "nv_research/output.hpp"
#include "nv_research/slot_layout.hpp"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace nv_research;

namespace {

int g_failures = 0;

#define check(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

template <typename t>
void put(std::vector<std::byte>& bytes, std::size_t offset, t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(t));
}

std::vector<std::byte> make_valid() {
    std::vector<std::byte> b(layout::g_logical_size);
    put<std::uint32_t>(b, layout::g_off_version, layout::g_expected_version);
    put<std::uint32_t>(b, layout::g_off_transport, 2);
    put<std::uint32_t>(b, layout::g_off_flags, 1);
    put<std::uint32_t>(b, layout::g_off_update_counter, 7);
    put<std::uint32_t>(b, layout::g_off_render_counter, 0x3003);
    put<std::uint64_t>(b, layout::g_off_generation, 99);
    put<std::uint32_t>(b, layout::g_off_width, 512);
    put<std::uint32_t>(b, layout::g_off_height, 320);
    put<float>(b, layout::g_off_alpha, 0.5f);
    return b;
}

void test_decode_valid() {
    const auto b = make_valid();
    const auto s = decode_slot(b.data(), b.size(), 3);
    check(s.m_status == e_slot_status::ok);
    check(s.m_has_fields);
    check(s.m_index == 3);
    check(s.m_transport == 2);
    check(s.m_update_counter == 7);
    check(s.m_generation == 99);
    check(s.m_width == 512 && s.m_height == 320);
}

void test_decode_truncated() {
    const auto b = make_valid();
    const auto tiny = decode_slot(b.data(), 2, 0);
    check(tiny.m_status == e_slot_status::insufficient_range);
    const auto partial = decode_slot(b.data(), layout::g_logical_size - 1, 0);
    check(partial.m_status == e_slot_status::insufficient_range);
    check(!partial.m_has_fields);
}

void test_decode_unknown_version() {
    auto b = make_valid();
    put<std::uint32_t>(b, layout::g_off_version, 0x12345678u);
    const auto s = decode_slot(b.data(), b.size(), 0);
    check(s.m_status == e_slot_status::unknown_version);
    check(s.m_version == 0x12345678u);
    check(!s.m_has_fields);
    check(s.m_width == 0);
}

void test_equivalent_and_changes() {
    auto b = make_valid();
    const auto a = decode_slot(b.data(), b.size(), 0);
    const auto same = decode_slot(b.data(), b.size(), 0);
    check(equivalent(a, same));
    put<std::uint32_t>(b, layout::g_off_update_counter, 8);
    const auto changed = decode_slot(b.data(), b.size(), 0);
    check(!equivalent(a, changed));
}

void test_json_output() {
    check(json_escape("a\"b\\c\n") == "a\\\"b\\\\c\\n");
    check(json_escape(std::string(1, '\x01')) == "\\u0001");
    check(json_float(std::numeric_limits<float>::quiet_NaN()) == "null");
    check(json_float(std::numeric_limits<float>::infinity()) == "null");
    check(json_float(0.5f) == "0.5");

    const auto b = make_valid();
    const auto s = decode_slot(b.data(), b.size(), 1);
    const std::string json = format_slot_json(s);
    check(json.find("\"status\":\"ok\"") != std::string::npos);
    check(json.find("\"width\":512") != std::string::npos);

    s_slot_sample unknown;
    unknown.m_status = e_slot_status::unknown_version;
    unknown.m_version = 5;
    check(format_slot_json(unknown).find("width") == std::string::npos);
}

void test_windows_mapping() {
    const std::wstring name = L"nv_research_test_mapping_" + std::to_wstring(GetCurrentProcessId());
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                        static_cast<DWORD>(layout::g_logical_size), name.c_str());
    check(mapping != nullptr);
    if (!mapping) return;
    void* view = MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, 0);
    check(view != nullptr);
    const auto b = make_valid();
    std::memcpy(view, b.data(), b.size());

    const auto s = read_slot(name, 0);
    check(s.m_status == e_slot_status::ok);
    check(s.m_generation == 99);

    check(std::memcmp(view, b.data(), b.size()) == 0);

    const auto sec = query_security(name, false);
    check(sec.m_available);
    check(!sec.m_owner_sid.empty());
    check(sec.m_sddl.find(L"O:") != std::wstring::npos);

    UnmapViewOfFile(view);
    CloseHandle(mapping);
}

void test_absent_mapping() {
    const auto s = read_slot(L"nv_research_test_does_not_exist_xyz", 0);
    check(s.m_status == e_slot_status::absent);
    check(s.m_error.m_code == ERROR_FILE_NOT_FOUND);
}

void test_process_context() {
    const auto ctx = query_processes(GetCurrentProcessId(), L"");
    check(ctx.m_processes.size() == 1);
    if (!ctx.m_processes.empty()) {
        check(ctx.m_processes[0].m_pid == GetCurrentProcessId());
        check(ctx.m_processes[0].m_creation_time.has_value());
        check(ctx.m_processes[0].m_session_id.has_value());
    }
}

}

int main() {
    test_decode_valid();
    test_decode_truncated();
    test_decode_unknown_version();
    test_equivalent_and_changes();
    test_json_output();
    test_windows_mapping();
    test_absent_mapping();
    test_process_context();
    if (g_failures) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("observer_tests: ok");
    return 0;
}
