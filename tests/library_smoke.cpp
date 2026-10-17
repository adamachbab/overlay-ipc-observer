#define nvovr_implementation
#include <nvovr.hpp>

#include <cstdio>
#include <string>
#include <type_traits>

#include "nv_research/slot_layout.hpp"

namespace t_layout = nv_research::layout;

int second_tu_check();

static int g_failures = 0;
#define smoke_check(cond) \
    do { if (!(cond)) { std::printf("FAIL: %s (line %d)\n", #cond, __LINE__); ++g_failures; } } while (0)

int main() {
    static_assert(t_layout::g_logical_size == 0x280, "logical size");
    static_assert(t_layout::g_slot_count == 8, "slot count");
    static_assert(t_layout::g_expected_version == 0x000B0280u, "version");
    static_assert(t_layout::g_off_version == 0x000, "off version");
    static_assert(t_layout::g_off_token_seed == 0x278, "off token seed");
    static_assert(t_layout::g_off_first_rect + 10 * sizeof(std::uint32_t) <= t_layout::g_off_alpha,
                  "rect array fits before alpha");
    static_assert(t_layout::g_format_bgra8 == 21, "format");

    smoke_check(t_layout::section_name(3) == std::wstring(t_layout::g_section_stem) + L"_3");
    smoke_check(t_layout::section_name(0) != t_layout::section_name(1));

    nvovr::s_rect rect{};
    smoke_check(rect.m_x == 0 && rect.m_y == 0 && rect.m_w == 0 && rect.m_h == 0);

    nvovr::s_placement placement{};
    smoke_check(placement.m_screen.m_w == 512 && placement.m_screen.m_h == 320);
    smoke_check(placement.m_scale_x == 1.0f && placement.m_alpha == 1.0f);

    nvovr::s_session_options opts{};
    smoke_check(opts.m_slot == nvovr::g_auto_slot);
    smoke_check(opts.m_nvidia_adapter_only);
    smoke_check(opts.m_resource_key_xor == 0);

    nvovr::s_slot_info info{};
    smoke_check(!info.m_present && !info.valid_version() && !info.gpu_transport() && !info.visible());
    info.m_version = static_cast<std::uint32_t>(nvovr_expected_version);
    info.m_transport = 2;
    info.m_flags = 1;
    smoke_check(info.valid_version() && info.gpu_transport() && info.visible());

    nvovr::s_token_diagnostics diag{};
    smoke_check(!diag.m_available && diag.m_folded_key == 0);

    static_assert(!std::is_copy_constructible_v<nvovr::c_session>, "session not copyable");
    static_assert(std::is_move_constructible_v<nvovr::c_session>, "session movable");
    static_assert(!std::is_copy_constructible_v<nvovr::c_compositor>, "compositor not copyable");
    static_assert(std::is_base_of_v<std::runtime_error, nvovr::c_error>, "error type");
    static_assert(std::is_same_v<decltype(nvovr::c_session::scan()), std::vector<nvovr::s_slot_info>>,
                  "scan() has no PID parameter");

    smoke_check(nvovr::process::find_first(L"nvovr_no_such_image_12345.exe") == 0);
    smoke_check(nvovr::process::find_all(L"nvovr_no_such_image_12345.exe").empty());

    try { throw nvovr::c_error("smoke"); }
    catch (const nvovr::c_error& e) { smoke_check(std::string(e.what()) == "smoke"); }

    smoke_check(second_tu_check() == 0);

    if (g_failures) { std::printf("%d failure(s)\n", g_failures); return 1; }
    std::printf("library_smoke: ok\n");
    return 0;
}
