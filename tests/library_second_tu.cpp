#include <nvovr.hpp>

#include <string>
#include <type_traits>

#include "nv_research/slot_layout.hpp"

int second_tu_check() {
    static_assert(nv_research::layout::g_expected_version == 0x000B0280u);
    static_assert(nv_research::layout::g_slot_count == 8);
    if (nv_research::layout::section_name(5) !=
        std::wstring(nv_research::layout::g_section_stem) + L"_5") return 3;

    nvovr::s_session_options opts{};
    opts.m_slot = 2;
    opts.m_placement.m_screen = nvovr::s_rect{10, 20, 64, 64};
    opts.m_placement.m_alpha = 0.5f;
    if (opts.m_placement.m_screen.m_w != 64) return 4;

    nvovr::s_slot_info row{};
    row.m_version = static_cast<std::uint32_t>(nvovr_expected_version);
    if (!row.valid_version()) return 5;

    using t_scan = std::vector<nvovr::s_slot_info> (*)();
    t_scan scan_fn = &nvovr::c_session::scan;
    if (!scan_fn) return 6;
    auto publish_ptr = &nvovr::c_session::publish;
    auto repost_ptr = &nvovr::c_session::repost;
    auto diag_ptr = &nvovr::c_session::token_diagnostics;
    (void)publish_ptr; (void)repost_ptr; (void)diag_ptr;
    return 0;
}
