# Naming conventions and nvovr migration

This document describes the snake_case conventions used across the project and the migration table from the original `nvovr.hpp` names. No compatibility aliases are kept.

## Conventions

| Identifier kind | Prefix | Example |
|---|---|---|
| Classes | `c_` | `c_session`, `c_compositor`, `c_error`, `c_imgui_host` |
| Structs | `s_` | `s_rect`, `s_placement`, `s_slot_info` |
| Data members (including public struct members) | `m_` | `m_screen`, `m_slot`, `m_alpha` |
| Globals and namespace-level constants | `g_` | `g_auto_slot`, `g_off_version` |
| Enums | `e_` | `e_slot_status`, `e_output_format` |
| Aliases and template parameters | `t_` | `t_value`, `t_scan` |
| Project macros | lowercase | `nvovr_implementation` |

Everything is lowercase with underscores. Public methods (`scan`, `attach`, `publish`, `repost`, `pid`, `slot`, ...) were already snake_case and keep their names. Windows, D3D, DXGI, ImGui and STL identifiers are not renamed.

## Macros

| Before | After |
|---|---|
| `NVOVR_IMPLEMENTATION` | `nvovr_implementation` |
| `NVOVR_ENABLE_IMGUI` | `nvovr_enable_imgui` |
| `NVOVR_EXPECTED_VERSION` | `nvovr_expected_version` |
| `NVOVR_HPP_INCLUDED` | `nvovr_hpp_included` |

`nvovr_expected_version` can be defined before the include to override the accepted version. By default it equals `nv_research::layout::g_expected_version` (`0x000B0280u`).

## Types

| Before | After |
|---|---|
| `Rect` | `s_rect` |
| `Placement` | `s_placement` |
| `SessionOptions` | `s_session_options` |
| `SlotInfo` | `s_slot_info` |
| `TokenDiagnostics` | `s_token_diagnostics` |
| `Error` | `c_error` |
| `Session` | `c_session` |
| `Compositor` | `c_compositor` |
| `Compositor::Frame` | `c_compositor::s_frame` |
| `ImGuiHost` | `c_imgui_host` |
| `Impl` (private) | `s_impl` |
| `HandleGuard` | `s_handle_guard` |
| `MapView` | `s_map_view` |
| `ComPtr<T>` | `s_com_ptr<t_value>` |
| `MutexLock` | `s_mutex_lock` |
| `SlotSnapshot` (private) | `s_slot_snapshot` |

## Constants and globals

| Before | After |
|---|---|
| `kAutoSlot` | `g_auto_slot` |
| `kMutexTimeout` | `g_mutex_timeout` |
| `kSectionStem` | `nv_research::layout::g_section_stem` |
| `kLogicalSize` | `nv_research::layout::g_logical_size` |
| `kSlotCount` | `nv_research::layout::g_slot_count` |
| `kOff*` (all field offsets) | `g_off_*` (e.g. `kOffUpdateCounter` -> `g_off_update_counter`) |
| `kFormatBgra8` | `g_format_bgra8` |

Offsets and names live in `include/nv_research/slot_layout.hpp`, namespace `nv_research::layout`, with no Windows or D3D dependencies. `nvovr.hpp` reuses them through `using` declarations in `nvovr::detail`.

## Data members

All public struct members gained the `m_` prefix, e.g. `Rect::x` -> `s_rect::m_x`, `Placement::screen` -> `s_placement::m_screen`, `SlotInfo::update_counter` -> `s_slot_info::m_update_counter`, `Compositor::Frame::rtv` -> `c_compositor::s_frame::m_rtv`. External code using them must be updated.

## Documentation corrections

- Section names do NOT include a PID: they are `stem + '_' + slot`.
- `c_session::scan()` takes no PID parameter; it only probes sections by slot index.
- `attach()` does NOT demonstrate association with any consumer, nor any driver or kernel code path.
- `c_compositor` holds only a reference to `c_session`; the session must outlive every compositor created from it.

## Tests

`tests/library_smoke.cpp` (implementation TU with `main`) and `tests/library_second_tu.cpp` (TU without the implementation) compile the renamed API and check the layout. They never run `attach()`, `publish()` or key recovery.
