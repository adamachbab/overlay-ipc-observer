#ifndef nvovr_hpp_included
#define nvovr_hpp_included

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include "nv_research/slot_layout.hpp"

#ifndef nvovr_expected_version
#define nvovr_expected_version ::nv_research::layout::g_expected_version
#endif

namespace nvovr {

namespace process {

DWORD find_first(const std::wstring& image_basename);

std::vector<DWORD> find_all(const std::wstring& image_basename);

DWORD wait_for(const std::wstring& image_basename,
               std::chrono::milliseconds timeout,
               std::chrono::milliseconds poll_interval = std::chrono::milliseconds{200});

}

struct s_rect {
    std::uint32_t m_x = 0;
    std::uint32_t m_y = 0;
    std::uint32_t m_w = 0;
    std::uint32_t m_h = 0;
};

struct s_placement {
    s_rect m_screen{80, 80, 512, 320};
    float  m_scale_x = 1.0f;
    float  m_scale_y = 1.0f;
    float  m_alpha   = 1.0f;
};

constexpr int g_auto_slot = -1;

struct s_session_options {
    int          m_slot                 = g_auto_slot;
    s_placement  m_placement            = {};
    std::wstring m_require_target_name  = {};
    bool         m_nvidia_adapter_only  = true;
    std::uint64_t m_resource_key_xor    = 0;
    bool          m_derive_resource_key_from_log = true;
};

struct s_slot_info {
    unsigned      m_index          = 0;
    bool          m_present        = false;
    std::uint32_t m_version        = 0;
    std::uint32_t m_transport      = 0;   // 2 == GPU shared surface
    std::uint32_t m_flags          = 0;   // bit0 => element is visible
    std::uint32_t m_update_counter = 0;
    std::uint32_t m_render_counter = 0;
    std::uint64_t m_generation     = 0;
    std::uint32_t m_width          = 0;
    std::uint32_t m_height         = 0;
    float         m_alpha          = 0.0f;

    bool valid_version() const { return m_version == nvovr_expected_version; }
    bool gpu_transport()  const { return m_transport == 2; }
    bool visible()        const { return (m_flags & 1u) != 0; }
};

struct s_token_diagnostics {
    bool          m_available          = false;
    bool          m_key_from_log       = false;
    bool          m_round_trip_ok      = false;
    std::uint64_t m_folded_key         = 0;
    std::uint32_t m_seed               = 0;
    std::uint64_t m_mask               = 0;
    std::uint64_t m_encoded_resource   = 0;
    std::uint64_t m_decoded_resource   = 0;
    std::uint64_t m_last_shared_handle = 0;
    std::uint32_t m_update_counter     = 0;
    std::uint32_t m_render_counter     = 0;
    std::uint64_t m_generation         = 0;
};

class c_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class c_compositor;

class c_session {
public:
    static std::vector<s_slot_info> scan();

    static c_session attach(DWORD pid, const s_session_options& opts = {});

    static c_session attach_by_name(
        const std::wstring& image_basename,
        s_session_options opts = {},
        std::chrono::milliseconds wait_timeout = std::chrono::milliseconds{0});

    c_session(c_session&&) noexcept;
    c_session& operator=(c_session&&) noexcept;
    ~c_session();
    c_session(const c_session&)            = delete;
    c_session& operator=(const c_session&) = delete;

    DWORD                pid()          const noexcept;
    HANDLE               process()      const noexcept;
    unsigned             slot()         const noexcept;
    std::wstring         target_image() const;
    const s_placement&   placement()    const noexcept;
    s_slot_info          current()      const;
    s_token_diagnostics  token_diagnostics() const;

    void publish(HANDLE shared_handle, std::uint32_t width, std::uint32_t height,
                 std::uint32_t pitch, const s_placement& placement);

    void repost(const s_placement* placement = nullptr);

private:
    struct s_impl;
    std::unique_ptr<s_impl> m_impl;

    friend class c_compositor;
    explicit c_session(std::unique_ptr<s_impl> impl) noexcept;
};

class c_compositor {
public:
    struct s_frame {
        ID3D11Device*           m_device;
        ID3D11DeviceContext*    m_context;
        ID3D11Texture2D*        m_texture;
        ID3D11RenderTargetView* m_rtv;
        std::uint32_t           m_width;
        std::uint32_t           m_height;
        float                   m_clear_color[4];
    };

    explicit c_compositor(c_session& session);
    ~c_compositor();
    c_compositor(const c_compositor&)            = delete;
    c_compositor& operator=(const c_compositor&) = delete;

    s_frame& begin_frame();
    void     present();

    template <typename t_fn>
    void render(t_fn&& fn) {
        auto& f = begin_frame();
        static_cast<t_fn&&>(fn)(f);
        present();
    }

    ID3D11Device*        device()  const noexcept;
    ID3D11DeviceContext* context() const noexcept;
    HANDLE               shared_handle() const noexcept;

    std::uint32_t width()  const noexcept;
    std::uint32_t height() const noexcept;

    void set_clear_color(float r, float g, float b, float a);

private:
    struct s_impl;
    std::unique_ptr<s_impl> m_impl;
};

#ifdef nvovr_enable_imgui
class c_imgui_host {
public:
    explicit c_imgui_host(c_compositor& comp);
    ~c_imgui_host();
    c_imgui_host(const c_imgui_host&)            = delete;
    c_imgui_host& operator=(const c_imgui_host&) = delete;

    void new_frame(float delta_seconds);

    void render();

private:
    struct s_impl;
    std::unique_ptr<s_impl> m_impl;
};
#endif

}

#ifdef nvovr_implementation

#include <chrono>
#include <cstring>
#include <filesystem>
#include <thread>
#include <utility>

#include <tlhelp32.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")

namespace nvovr {
namespace detail {

using ::nv_research::layout::g_section_stem;
using ::nv_research::layout::g_logical_size;
using ::nv_research::layout::g_slot_count;
using ::nv_research::layout::g_off_version;
using ::nv_research::layout::g_off_update_counter;
using ::nv_research::layout::g_off_transport;
using ::nv_research::layout::g_off_generation;
using ::nv_research::layout::g_off_shared_resource;
using ::nv_research::layout::g_off_format;
using ::nv_research::layout::g_off_width;
using ::nv_research::layout::g_off_height;
using ::nv_research::layout::g_off_pitch;
using ::nv_research::layout::g_off_abs_x;
using ::nv_research::layout::g_off_abs_y;
using ::nv_research::layout::g_off_scale_x;
using ::nv_research::layout::g_off_scale_y;
using ::nv_research::layout::g_off_flags;
using ::nv_research::layout::g_off_render_counter;
using ::nv_research::layout::g_off_rect_count;
using ::nv_research::layout::g_off_first_rect;
using ::nv_research::layout::g_off_alpha;
using ::nv_research::layout::g_off_token_seed;
using ::nv_research::layout::g_format_bgra8;
using ::nv_research::layout::section_name;

inline constexpr std::uint32_t g_mutex_timeout = 2000;

struct s_handle_guard {
    HANDLE m_h = nullptr;
    s_handle_guard() = default;
    explicit s_handle_guard(HANDLE handle) noexcept : m_h(handle) {}
    s_handle_guard(s_handle_guard&& o) noexcept : m_h(std::exchange(o.m_h, nullptr)) {}
    s_handle_guard& operator=(s_handle_guard&& o) noexcept {
        if (this != &o) { reset(); m_h = std::exchange(o.m_h, nullptr); }
        return *this;
    }
    s_handle_guard(const s_handle_guard&)            = delete;
    s_handle_guard& operator=(const s_handle_guard&) = delete;
    ~s_handle_guard() { reset(); }
    void reset() {
        if (m_h && m_h != INVALID_HANDLE_VALUE) CloseHandle(m_h);
        m_h = nullptr;
    }
    HANDLE release() noexcept { return std::exchange(m_h, nullptr); }
    explicit operator bool() const noexcept { return m_h && m_h != INVALID_HANDLE_VALUE; }
};

struct s_map_view {
    void* m_p = nullptr;
    s_map_view() = default;
    explicit s_map_view(void* ptr) noexcept : m_p(ptr) {}
    s_map_view(s_map_view&& o) noexcept : m_p(std::exchange(o.m_p, nullptr)) {}
    s_map_view& operator=(s_map_view&& o) noexcept {
        if (this != &o) { reset(); m_p = std::exchange(o.m_p, nullptr); }
        return *this;
    }
    s_map_view(const s_map_view&)            = delete;
    s_map_view& operator=(const s_map_view&) = delete;
    ~s_map_view() { reset(); }
    void reset() { if (m_p) UnmapViewOfFile(m_p); m_p = nullptr; }
};

template <typename t_value>
struct s_com_ptr {
    t_value* m_p = nullptr;
    s_com_ptr() = default;
    ~s_com_ptr() { if (m_p) m_p->Release(); }
    s_com_ptr(const s_com_ptr&)            = delete;
    s_com_ptr& operator=(const s_com_ptr&) = delete;
    s_com_ptr(s_com_ptr&& o) noexcept : m_p(std::exchange(o.m_p, nullptr)) {}
    s_com_ptr& operator=(s_com_ptr&& o) noexcept {
        if (this != &o) { if (m_p) m_p->Release(); m_p = std::exchange(o.m_p, nullptr); }
        return *this;
    }
    t_value** put() { if (m_p) { m_p->Release(); m_p = nullptr; } return &m_p; }
    t_value*  get() const noexcept { return m_p; }
    t_value*  operator->() const noexcept { return m_p; }
    explicit operator bool() const noexcept { return m_p != nullptr; }
};

struct s_mutex_lock {
    HANDLE m_h = nullptr;
    bool   m_held = false;
    explicit s_mutex_lock(HANDLE handle) : m_h(handle) {
        const DWORD r = WaitForSingleObject(m_h, g_mutex_timeout);
        if (r != WAIT_OBJECT_0 && r != WAIT_ABANDONED)
            throw c_error("nvovr: timed out acquiring overlay mutex");
        m_held = true;
    }
    ~s_mutex_lock() { if (m_held) ReleaseMutex(m_h); }
    s_mutex_lock(const s_mutex_lock&)            = delete;
    s_mutex_lock& operator=(const s_mutex_lock&) = delete;
};

template <typename t_value>
inline t_value mem_read(const std::byte* b, std::size_t off) noexcept {
    t_value v{}; std::memcpy(&v, b + off, sizeof(t_value)); return v;
}
template <typename t_value>
inline void mem_write(std::byte* b, std::size_t off, const t_value& v) noexcept {
    std::memcpy(b + off, &v, sizeof(t_value));
}

inline std::uint64_t resource_token_mask(std::uint64_t key_xor,
                                         std::uint32_t seed) noexcept {
    constexpr std::uint64_t c1 = 0xFF51AFD7ED558CCDull;
    constexpr std::uint64_t cx = 0xED77E7F1C90AA277ull;
    constexpr std::uint64_t c2 = 0xC4CEB9FE1A85EC53ull;
    const std::uint64_t repeated = static_cast<std::uint64_t>(seed) |
        (static_cast<std::uint64_t>(seed) << 32);
    std::uint64_t value = key_xor ^ repeated;
    value = (value ^ (value >> 33)) * c1;
    value = (value ^ (value >> 33)) * cx;
    value = (value ^ (value >> 33)) * c2;
    return value ^ (value >> 33);
}

inline std::uint64_t undo_xor_shift33(std::uint64_t value) noexcept {
    return value ^ (value >> 33);
}

inline std::uint64_t recover_resource_key(std::uint64_t mask,
                                          std::uint32_t seed) noexcept {
    constexpr std::uint64_t inv_c1 = 0x4F74430C22A54005ull;
    constexpr std::uint64_t inv_cx = 0xD361F90F40FDD747ull;
    constexpr std::uint64_t inv_c2 = 0x9CB4B2F8129337DBull;
    std::uint64_t value = undo_xor_shift33(mask);
    value = undo_xor_shift33(value * inv_c2);
    value = undo_xor_shift33(value * inv_cx);
    value = undo_xor_shift33(value * inv_c1);
    const std::uint64_t repeated = static_cast<std::uint64_t>(seed) |
        (static_cast<std::uint64_t>(seed) << 32);
    return value ^ repeated;
}

inline std::optional<std::uint64_t> latest_decoded_resource(DWORD pid,
                                                            unsigned slot) {
    wchar_t program_data[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(
        L"ProgramData", program_data, static_cast<DWORD>(std::size(program_data)));
    if (!length || length >= std::size(program_data)) return std::nullopt;
    const auto log_path = std::filesystem::path(program_data) /
        L"NVIDIA Corporation" / L"ShadowPlay" / L"CaptureCore.log";
    std::ifstream input(log_path, std::ios::binary);
    if (!input) return std::nullopt;

    const std::string pid_marker = ":" + std::to_string(pid) + ":";
    const std::string slot_marker = "slot[" + std::to_string(slot) + "]";
    constexpr char resource_marker[] = "hResource[0x";
    std::optional<std::uint64_t> latest;
    std::string line;
    while (std::getline(input, line)) {
        if (line.find(pid_marker) == std::string::npos ||
            line.find(slot_marker) == std::string::npos)
            continue;
        const auto at = line.find(resource_marker);
        if (at == std::string::npos) continue;
        const auto begin = at + sizeof(resource_marker) - 1;
        const auto end = line.find_first_not_of("0123456789abcdefABCDEF", begin);
        const auto digits = line.substr(begin, end - begin);
        if (digits.empty() || digits.size() > 16) continue;
        try {
            const std::uint64_t value = std::stoull(digits, nullptr, 16);
            if (value) latest = value;
        } catch (const std::exception&) {
        }
    }
    return latest;
}

inline std::wstring process_image(HANDLE process) {
    std::vector<wchar_t> path(32768);
    DWORD len = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &len))
        throw c_error("nvovr: QueryFullProcessImageNameW failed");
    return std::wstring(path.data(), len);
}

inline s_slot_info probe_slot(unsigned slot) {
    s_slot_info si{};
    si.m_index = slot;
    const auto name = section_name(slot);
    HANDLE mh = OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str());
    if (!mh) return si;
    s_handle_guard mapping{mh};
    void* v = MapViewOfFile(mapping.m_h, FILE_MAP_READ, 0, 0, 0);
    if (!v) return si;
    s_map_view view{v};
    auto* b = static_cast<const std::byte*>(v);
    si.m_present        = true;
    si.m_version        = mem_read<std::uint32_t>(b, g_off_version);
    si.m_transport      = mem_read<std::uint32_t>(b, g_off_transport);
    si.m_flags          = mem_read<std::uint32_t>(b, g_off_flags);
    si.m_update_counter = mem_read<std::uint32_t>(b, g_off_update_counter);
    si.m_render_counter = mem_read<std::uint32_t>(b, g_off_render_counter);
    si.m_generation     = mem_read<std::uint64_t>(b, g_off_generation);
    si.m_width          = mem_read<std::uint32_t>(b, g_off_width);
    si.m_height         = mem_read<std::uint32_t>(b, g_off_height);
    si.m_alpha          = mem_read<float        >(b, g_off_alpha);
    return si;
}

struct s_slot_snapshot {
    std::uint32_t m_update{};
    std::uint64_t m_generation{};
    std::uint64_t m_resource{};
    std::uint32_t m_format{};
    std::uint32_t m_width{};
    std::uint32_t m_height{};
    std::uint32_t m_pitch{};
    std::uint32_t m_abs_x{};
    std::uint32_t m_abs_y{};
    float         m_scale_x{};
    float         m_scale_y{};
    std::uint32_t m_flags{};
    std::uint32_t m_render{};
    std::uint32_t m_rects{};
    std::array<std::uint32_t, 10> m_first_rect{};
    float         m_alpha{};
};

inline s_slot_snapshot capture(const std::byte* b) {
    return { mem_read<std::uint32_t>(b, g_off_update_counter),
             mem_read<std::uint64_t>(b, g_off_generation),
             mem_read<std::uint64_t>(b, g_off_shared_resource),
             mem_read<std::uint32_t>(b, g_off_format),
             mem_read<std::uint32_t>(b, g_off_width),
             mem_read<std::uint32_t>(b, g_off_height),
             mem_read<std::uint32_t>(b, g_off_pitch),
             mem_read<std::uint32_t>(b, g_off_abs_x),
             mem_read<std::uint32_t>(b, g_off_abs_y),
             mem_read<float        >(b, g_off_scale_x),
             mem_read<float        >(b, g_off_scale_y),
             mem_read<std::uint32_t>(b, g_off_flags),
             mem_read<std::uint32_t>(b, g_off_render_counter),
             mem_read<std::uint32_t>(b, g_off_rect_count),
             mem_read<std::array<std::uint32_t, 10>>(b, g_off_first_rect),
             mem_read<float        >(b, g_off_alpha) };
}

inline void restore(std::byte* b, const s_slot_snapshot& s, std::uint32_t update) {
    mem_write(b, g_off_generation,     s.m_generation);
    mem_write(b, g_off_shared_resource, s.m_resource);
    mem_write(b, g_off_format,         s.m_format);
    mem_write(b, g_off_width,          s.m_width);
    mem_write(b, g_off_height,         s.m_height);
    mem_write(b, g_off_pitch,          s.m_pitch);
    mem_write(b, g_off_abs_x,          s.m_abs_x);
    mem_write(b, g_off_abs_y,          s.m_abs_y);
    mem_write(b, g_off_scale_x,        s.m_scale_x);
    mem_write(b, g_off_scale_y,        s.m_scale_y);
    mem_write(b, g_off_flags,          s.m_flags);
    mem_write(b, g_off_render_counter, s.m_render);
    mem_write(b, g_off_rect_count,     s.m_rects);
    mem_write(b, g_off_first_rect,     s.m_first_rect);
    mem_write(b, g_off_alpha,          s.m_alpha);
    MemoryBarrier();
    mem_write(b, g_off_update_counter, update);
}

}

namespace process {

DWORD find_first(const std::wstring& image_basename) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    DWORD hit = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, image_basename.c_str()) == 0) {
                hit = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return hit;
}

std::vector<DWORD> find_all(const std::wstring& image_basename) {
    std::vector<DWORD> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, image_basename.c_str()) == 0)
                out.push_back(pe.th32ProcessID);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return out;
}

DWORD wait_for(const std::wstring& image_basename,
                      std::chrono::milliseconds timeout,
                      std::chrono::milliseconds poll_interval) {
    using t_clock = std::chrono::steady_clock;
    const auto deadline = t_clock::now() + timeout;
    for (;;) {
        DWORD pid = find_first(image_basename);
        if (pid) return pid;
        if (t_clock::now() >= deadline) return 0;
        std::this_thread::sleep_for(poll_interval);
    }
}

}

struct c_session::s_impl {
    DWORD                      m_pid           = 0;
    unsigned                   m_slot          = 0;
    detail::s_handle_guard     m_process;
    detail::s_handle_guard     m_mapping;
    detail::s_map_view         m_view;
    detail::s_handle_guard     m_mutex;
    std::byte*                 m_bytes         = nullptr;
    detail::s_slot_snapshot    m_original      = {};
    bool                       m_restored      = false;
    s_placement                m_placement     = {};
    std::uint64_t              m_resource_key_xor = 0;
    bool                       m_resource_key_from_log = false;

    std::uint64_t              m_gen_cursor    = 0;
    std::uint32_t              m_render_cursor = 0;
    std::uint64_t              m_last_handle   = 0;
    std::uint32_t              m_last_width    = 0;
    std::uint32_t              m_last_height   = 0;
    std::uint32_t              m_last_pitch    = 0;

    void restore_now() {
        if (m_restored || !m_bytes) return;
        try {
            detail::s_mutex_lock lk{m_mutex.m_h};
            std::uint32_t cur = detail::mem_read<std::uint32_t>(m_bytes, detail::g_off_update_counter);
            std::uint32_t next = (cur == UINT32_MAX) ? m_original.m_update : cur + 1;
            detail::restore(m_bytes, m_original, next);
            FlushViewOfFile(m_bytes, detail::g_logical_size);
        } catch (...) {}
        m_restored = true;
    }
};

c_session::c_session(std::unique_ptr<s_impl> impl) noexcept : m_impl(std::move(impl)) {}
c_session::c_session(c_session&&) noexcept            = default;
c_session& c_session::operator=(c_session&&) noexcept = default;

c_session::~c_session() {
    if (m_impl) m_impl->restore_now();
}

std::vector<s_slot_info> c_session::scan() {
    std::vector<s_slot_info> rows;
    rows.reserve(detail::g_slot_count);
    for (unsigned i = 0; i < detail::g_slot_count; ++i)
        rows.push_back(detail::probe_slot(i));
    return rows;
}

c_session c_session::attach(DWORD pid, const s_session_options& opts) {
    if (!pid) throw c_error("nvovr: pid == 0");

    auto impl = std::make_unique<s_impl>();
    impl->m_pid       = pid;
    impl->m_placement = opts.m_placement;
    impl->m_resource_key_xor = opts.m_resource_key_xor;

    impl->m_process = detail::s_handle_guard{OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid)};
    if (!impl->m_process) throw c_error("nvovr: OpenProcess(query-only) failed");

    if (!opts.m_require_target_name.empty()) {
        const auto image = detail::process_image(impl->m_process.m_h);
        const auto base  = std::filesystem::path(image).filename().wstring();
        if (_wcsicmp(base.c_str(), opts.m_require_target_name.c_str()) != 0)
            throw c_error("nvovr: target image name mismatch");
    }

    int chosen = opts.m_slot;
    if (chosen == g_auto_slot) {
        int best = -1;
        std::uint64_t best_score = 0;
        for (unsigned i = 0; i < detail::g_slot_count; ++i) {
            auto si = detail::probe_slot(i);
            if (!si.m_present || !si.valid_version() || !si.gpu_transport()) continue;
            const std::uint64_t score = (static_cast<std::uint64_t>(si.visible()) << 40)
                                        | si.m_render_counter;
            if (best < 0 || score > best_score) {
                best_score = score;
                best = static_cast<int>(i);
            }
        }
        if (best < 0) throw c_error("nvovr: no live GPU-transport slot found");
        chosen = best;
    }
    if (chosen < 0 || chosen >= static_cast<int>(detail::g_slot_count))
        throw c_error("nvovr: invalid slot index");
    impl->m_slot = static_cast<unsigned>(chosen);

    const auto sec = detail::section_name(impl->m_slot);
    const DWORD access = FILE_MAP_READ | FILE_MAP_WRITE;
    impl->m_mapping = detail::s_handle_guard{OpenFileMappingW(access, FALSE, sec.c_str())};
    if (!impl->m_mapping) throw c_error("nvovr: OpenFileMappingW failed");
    void* v = MapViewOfFile(impl->m_mapping.m_h, access, 0, 0, 0);
    if (!v) throw c_error("nvovr: MapViewOfFile failed");
    impl->m_view  = detail::s_map_view{v};
    impl->m_bytes = static_cast<std::byte*>(v);

    const std::uint32_t ver = detail::mem_read<std::uint32_t>(impl->m_bytes, detail::g_off_version);
    if (ver != nvovr_expected_version)
        throw c_error("nvovr: section version mismatch (rebuild with nvovr_expected_version)");
    const std::uint32_t tr = detail::mem_read<std::uint32_t>(impl->m_bytes, detail::g_off_transport);
    if (tr != 2)
        throw c_error("nvovr: slot not using GPU shared-surface transport");

    impl->m_mutex = detail::s_handle_guard{OpenMutexW(
        SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (sec + L"_0").c_str())};
    if (!impl->m_mutex) throw c_error("nvovr: OpenMutexW failed");

    {
        detail::s_mutex_lock lk{impl->m_mutex.m_h};
        impl->m_original = detail::capture(impl->m_bytes);
        if (impl->m_original.m_update == UINT32_MAX ||
            impl->m_original.m_render == UINT32_MAX ||
            impl->m_original.m_generation == UINT64_MAX)
            throw c_error("nvovr: counter about to roll over - refusing to attach");
        impl->m_gen_cursor    = impl->m_original.m_generation;
        impl->m_render_cursor = impl->m_original.m_render & 0xFFFFF000u;
        if (!impl->m_resource_key_xor && opts.m_derive_resource_key_from_log) {
            const auto decoded = detail::latest_decoded_resource(pid, impl->m_slot);
            const auto seed = detail::mem_read<std::uint32_t>(
                impl->m_bytes, detail::g_off_token_seed);
            if (decoded && impl->m_original.m_resource && seed) {
                const auto mask = impl->m_original.m_resource ^ *decoded;
                impl->m_resource_key_xor = detail::recover_resource_key(mask, seed);
                impl->m_resource_key_from_log = true;
            }
        }
        if (!impl->m_resource_key_xor)
            throw c_error("nvovr: v9 resource-token key unavailable");
    }

    return c_session{std::move(impl)};
}

c_session c_session::attach_by_name(const std::wstring& image_basename,
                                           s_session_options opts,
                                           std::chrono::milliseconds wait_timeout) {
    if (image_basename.empty())
        throw c_error("nvovr: attach_by_name got empty image basename");
    if (opts.m_require_target_name.empty()) opts.m_require_target_name = image_basename;

    DWORD pid = wait_timeout.count() > 0
        ? process::wait_for(image_basename, wait_timeout)
        : process::find_first(image_basename);
    if (!pid) throw c_error("nvovr: target process not found");
    return attach(pid, opts);
}

DWORD              c_session::pid()          const noexcept { return m_impl->m_pid; }
HANDLE             c_session::process()      const noexcept { return m_impl->m_process.m_h; }
unsigned           c_session::slot()         const noexcept { return m_impl->m_slot; }
std::wstring       c_session::target_image() const           { return detail::process_image(m_impl->m_process.m_h); }
const s_placement& c_session::placement()    const noexcept { return m_impl->m_placement; }

s_slot_info c_session::current() const {
    return detail::probe_slot(m_impl->m_slot);
}

s_token_diagnostics c_session::token_diagnostics() const {
    s_token_diagnostics out{};
    if (!m_impl || !m_impl->m_bytes || !m_impl->m_resource_key_xor) return out;

    detail::s_mutex_lock lk{m_impl->m_mutex.m_h};
    out.m_available = true;
    out.m_key_from_log = m_impl->m_resource_key_from_log;
    out.m_folded_key = m_impl->m_resource_key_xor;
    out.m_seed = detail::mem_read<std::uint32_t>(m_impl->m_bytes, detail::g_off_token_seed);
    out.m_encoded_resource = detail::mem_read<std::uint64_t>(
        m_impl->m_bytes, detail::g_off_shared_resource);
    out.m_update_counter = detail::mem_read<std::uint32_t>(
        m_impl->m_bytes, detail::g_off_update_counter);
    out.m_render_counter = detail::mem_read<std::uint32_t>(
        m_impl->m_bytes, detail::g_off_render_counter);
    out.m_generation = detail::mem_read<std::uint64_t>(
        m_impl->m_bytes, detail::g_off_generation);
    out.m_last_shared_handle = m_impl->m_last_handle;
    if (out.m_seed) {
        out.m_mask = detail::resource_token_mask(out.m_folded_key, out.m_seed);
        out.m_decoded_resource = out.m_encoded_resource ^ out.m_mask;
        out.m_round_trip_ok = out.m_last_shared_handle != 0 &&
            out.m_decoded_resource == out.m_last_shared_handle;
    }
    return out;
}

void c_session::publish(HANDLE shared_handle, std::uint32_t width, std::uint32_t height,
                               std::uint32_t pitch, const s_placement& p) {
    if (!shared_handle) throw c_error("nvovr: publish() got null shared handle");
    detail::s_mutex_lock lk{m_impl->m_mutex.m_h};

    ++m_impl->m_gen_cursor;
    if (m_impl->m_render_cursor > UINT32_MAX - 0x1000u)
        throw c_error("nvovr: render generation about to roll over");
    m_impl->m_render_cursor += 0x1000u;

    auto* b = m_impl->m_bytes;
    detail::mem_write(b, detail::g_off_generation, m_impl->m_gen_cursor);
    const std::uint32_t token_seed = detail::mem_read<std::uint32_t>(
        b, detail::g_off_token_seed);
    if (!token_seed) throw c_error("nvovr: v9 resource-token seed unavailable");
    const std::uint64_t encoded_resource =
        reinterpret_cast<std::uint64_t>(shared_handle) ^
        detail::resource_token_mask(m_impl->m_resource_key_xor, token_seed);
    detail::mem_write(b, detail::g_off_shared_resource, encoded_resource);
    detail::mem_write(b, detail::g_off_format, detail::g_format_bgra8);
    detail::mem_write(b, detail::g_off_width,  width);
    detail::mem_write(b, detail::g_off_height, height);
    detail::mem_write(b, detail::g_off_pitch,  pitch);
    detail::mem_write(b, detail::g_off_abs_x,  p.m_screen.m_x);
    detail::mem_write(b, detail::g_off_abs_y,  p.m_screen.m_y);
    detail::mem_write(b, detail::g_off_scale_x, p.m_scale_x);
    detail::mem_write(b, detail::g_off_scale_y, p.m_scale_y);
    detail::mem_write(b, detail::g_off_flags,  m_impl->m_original.m_flags | 1u);

    const std::array<std::uint32_t, 10> rect_full = {
        0, 0, 0, width, height,
        0, 0, 0, width, height };
    detail::mem_write(b, detail::g_off_rect_count, std::uint32_t{1});
    detail::mem_write(b, detail::g_off_first_rect, rect_full);

    detail::mem_write(b, detail::g_off_render_counter, m_impl->m_render_cursor | 3u);
    detail::mem_write(b, detail::g_off_alpha, p.m_alpha);

    std::uint32_t u = detail::mem_read<std::uint32_t>(b, detail::g_off_update_counter);
    if (u == UINT32_MAX) u = 0;
    MemoryBarrier();
    detail::mem_write(b, detail::g_off_update_counter, u + 1);

    m_impl->m_placement   = p;
    m_impl->m_last_handle = reinterpret_cast<std::uint64_t>(shared_handle);
    m_impl->m_last_width  = width;
    m_impl->m_last_height = height;
    m_impl->m_last_pitch  = pitch;
}

void c_session::repost(const s_placement* p) {
    if (!m_impl->m_last_handle) throw c_error("nvovr: repost() before first publish()");
    const s_placement pl = p ? *p : m_impl->m_placement;
    publish(reinterpret_cast<HANDLE>(m_impl->m_last_handle),
            m_impl->m_last_width, m_impl->m_last_height, m_impl->m_last_pitch, pl);
}

struct c_compositor::s_impl {
    c_session*                              m_session = nullptr;
    detail::s_com_ptr<IDXGIFactory1>        m_factory;
    detail::s_com_ptr<IDXGIAdapter1>        m_adapter;
    detail::s_com_ptr<ID3D11Device>         m_device;
    detail::s_com_ptr<ID3D11DeviceContext>  m_context;
    detail::s_com_ptr<ID3D11Texture2D>      m_texture;
    detail::s_com_ptr<IDXGIResource>        m_resource;
    detail::s_com_ptr<ID3D11RenderTargetView> m_rtv;
    HANDLE                                  m_shared_handle = nullptr;
    std::uint32_t                           m_width  = 0;
    std::uint32_t                           m_height = 0;
    float                                   m_clear_color[4] = {0.f, 0.f, 0.f, 0.f};
    s_frame                                 m_frame{};

    void build_device(bool nv_only) {
        HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1),
            reinterpret_cast<void**>(m_factory.put()));
        if (FAILED(hr)) throw c_error("nvovr: CreateDXGIFactory1 failed");

        for (UINT i = 0;; ++i) {
            IDXGIAdapter1* cand = nullptr;
            hr = m_factory->EnumAdapters1(i, &cand);
            if (hr == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(hr)) throw c_error("nvovr: EnumAdapters1 failed");
            DXGI_ADAPTER_DESC1 d{};
            cand->GetDesc1(&d);
            const bool pick = nv_only ? (d.VendorId == 0x10DE) : true;
            if (pick) { m_adapter.put(); m_adapter.m_p = cand; break; }
            cand->Release();
        }
        if (!m_adapter && nv_only) throw c_error("nvovr: no NVIDIA DXGI adapter");
        if (!m_adapter) throw c_error("nvovr: no DXGI adapter");

        D3D_FEATURE_LEVEL got{};
        const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
        hr = D3D11CreateDevice(m_adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
            static_cast<UINT>(std::size(levels)),
            D3D11_SDK_VERSION, m_device.put(), &got, m_context.put());
        if (hr == E_INVALIDARG) {
            hr = D3D11CreateDevice(m_adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels + 1,
                static_cast<UINT>(std::size(levels)) - 1,
                D3D11_SDK_VERSION, m_device.put(), &got, m_context.put());
        }
        if (FAILED(hr)) throw c_error("nvovr: D3D11CreateDevice failed");
    }

    void build_texture() {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = m_width; td.Height = m_height;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        td.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        HRESULT hr = m_device->CreateTexture2D(&td, nullptr, m_texture.put());
        if (FAILED(hr)) throw c_error("nvovr: CreateTexture2D(shared) failed");

        hr = m_texture->QueryInterface(__uuidof(IDXGIResource),
            reinterpret_cast<void**>(m_resource.put()));
        if (FAILED(hr)) throw c_error("nvovr: IDXGIResource query failed");
        hr = m_resource->GetSharedHandle(&m_shared_handle);
        if (FAILED(hr) || !m_shared_handle) throw c_error("nvovr: GetSharedHandle failed");

        D3D11_RENDER_TARGET_VIEW_DESC rvd{};
        rvd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        rvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        hr = m_device->CreateRenderTargetView(m_texture.get(), &rvd, m_rtv.put());
        if (FAILED(hr)) throw c_error("nvovr: CreateRenderTargetView failed");
    }
};

c_compositor::c_compositor(c_session& session) : m_impl(std::make_unique<s_impl>()) {
    m_impl->m_session = &session;
    m_impl->m_width   = session.placement().m_screen.m_w;
    m_impl->m_height  = session.placement().m_screen.m_h;
    if (m_impl->m_width < 8 || m_impl->m_height < 8)
        throw c_error("nvovr: placement size must be >= 8x8");

    m_impl->build_device(true);
    m_impl->build_texture();
}

c_compositor::~c_compositor() = default;

c_compositor::s_frame& c_compositor::begin_frame() {
    auto& f = m_impl->m_frame;
    f.m_device  = m_impl->m_device.get();
    f.m_context = m_impl->m_context.get();
    f.m_texture = m_impl->m_texture.get();
    f.m_rtv     = m_impl->m_rtv.get();
    f.m_width   = m_impl->m_width;
    f.m_height  = m_impl->m_height;
    std::memcpy(f.m_clear_color, m_impl->m_clear_color, sizeof(f.m_clear_color));

    ID3D11RenderTargetView* rtvs[] = { m_impl->m_rtv.get() };
    m_impl->m_context->OMSetRenderTargets(1, rtvs, nullptr);
    D3D11_VIEWPORT vp{};
    vp.Width  = static_cast<float>(m_impl->m_width);
    vp.Height = static_cast<float>(m_impl->m_height);
    vp.MaxDepth = 1.0f;
    m_impl->m_context->RSSetViewports(1, &vp);
    m_impl->m_context->ClearRenderTargetView(m_impl->m_rtv.get(), f.m_clear_color);
    return f;
}

void c_compositor::present() {
    m_impl->m_context->Flush();
    const std::uint32_t pitch = m_impl->m_width * 4;
    m_impl->m_session->publish(m_impl->m_shared_handle, m_impl->m_width, m_impl->m_height,
                               pitch, m_impl->m_session->placement());
}

ID3D11Device*        c_compositor::device()        const noexcept { return m_impl->m_device.get(); }
ID3D11DeviceContext* c_compositor::context()       const noexcept { return m_impl->m_context.get(); }
HANDLE               c_compositor::shared_handle() const noexcept { return m_impl->m_shared_handle; }
std::uint32_t        c_compositor::width()         const noexcept { return m_impl->m_width; }
std::uint32_t        c_compositor::height()        const noexcept { return m_impl->m_height; }

void c_compositor::set_clear_color(float r, float g, float b, float a) {
    m_impl->m_clear_color[0] = r; m_impl->m_clear_color[1] = g;
    m_impl->m_clear_color[2] = b; m_impl->m_clear_color[3] = a;
}

#ifdef nvovr_enable_imgui
struct c_imgui_host::s_impl {
    c_compositor* m_comp = nullptr;
    ImGuiContext* m_ctx = nullptr;
    bool          m_own_ctx = false;
};

c_imgui_host::c_imgui_host(c_compositor& comp) : m_impl(std::make_unique<s_impl>()) {
    m_impl->m_comp = &comp;
    if (!ImGui::GetCurrentContext()) {
        m_impl->m_ctx = ImGui::CreateContext();
        m_impl->m_own_ctx = true;
    } else {
        m_impl->m_ctx = ImGui::GetCurrentContext();
    }
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(static_cast<float>(comp.width()),
                            static_cast<float>(comp.height()));
    ImGui::StyleColorsDark();
    if (!ImGui_ImplDX11_Init(comp.device(), comp.context()))
        throw c_error("nvovr: ImGui_ImplDX11_Init failed");
}

c_imgui_host::~c_imgui_host() {
    ImGui_ImplDX11_Shutdown();
    if (m_impl->m_own_ctx) ImGui::DestroyContext(m_impl->m_ctx);
}

void c_imgui_host::new_frame(float dt) {
    ImGui_ImplDX11_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(m_impl->m_comp->width()),
                            static_cast<float>(m_impl->m_comp->height()));
    io.DeltaTime = dt > 0.0f ? dt : 1.0f / 60.0f;
    ImGui::NewFrame();
}

void c_imgui_host::render() {
    ImGui::Render();
    auto& f = m_impl->m_comp->begin_frame();
    (void)f;
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_impl->m_comp->present();
}
#endif

}

#endif
#endif
