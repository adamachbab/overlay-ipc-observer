#ifndef nv_research_slot_layout_hpp_included
#define nv_research_slot_layout_hpp_included

#include <cstddef>
#include <cstdint>
#include <string>

namespace nv_research {
namespace layout {

inline constexpr wchar_t g_section_stem[] =
    L"{1BBC83F3-72CB-453B-AD96-E1028AFFB5E9}_v9";
inline constexpr std::size_t g_logical_size = 0x280;
inline constexpr std::uint32_t g_slot_count = 8;
inline constexpr std::uint32_t g_expected_version = 0x000B0280u;

inline constexpr std::size_t g_off_version = 0x000;
inline constexpr std::size_t g_off_update_counter = 0x008;
inline constexpr std::size_t g_off_transport = 0x00C;
inline constexpr std::size_t g_off_generation = 0x010;
inline constexpr std::size_t g_off_shared_resource = 0x018;
inline constexpr std::size_t g_off_format = 0x020;
inline constexpr std::size_t g_off_width = 0x024;
inline constexpr std::size_t g_off_height = 0x028;
inline constexpr std::size_t g_off_pitch = 0x02C;
inline constexpr std::size_t g_off_abs_x = 0x048;
inline constexpr std::size_t g_off_abs_y = 0x04C;
inline constexpr std::size_t g_off_scale_x = 0x050;
inline constexpr std::size_t g_off_scale_y = 0x054;
inline constexpr std::size_t g_off_flags = 0x058;
inline constexpr std::size_t g_off_render_counter = 0x124;
inline constexpr std::size_t g_off_rect_count = 0x130;
inline constexpr std::size_t g_off_first_rect = 0x134;
inline constexpr std::size_t g_off_alpha = 0x274;
inline constexpr std::size_t g_off_token_seed = 0x278;

inline constexpr std::uint32_t g_format_bgra8 = 21;

inline std::wstring section_name(unsigned slot) {
    return std::wstring(g_section_stem) + L'_' + std::to_wstring(slot);
}

}
}

#endif
