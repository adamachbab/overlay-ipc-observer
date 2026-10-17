# nv_research

A Windows research project on the inter-process communication (IPC) used by the NVIDIA in-game overlay. It contains two independent parts:

- **`nv_research`**: a strictly read-only command-line observer for the overlay's named shared-memory slots.
- **`nvovr.hpp`**: a single-header C++17 library that can publish a shared D3D11 texture into an overlay slot. It is kept separate from the observer and is never linked into it.

> **Status:** early research artifact. The slot layout was reverse-engineered empirically for one driver version (section header `0x000B0280`). Newer drivers may use a different layout, name or namespace, in which case slots are reported as `absent` or `unknown_version`. Use it only on systems you own, with software you control.

## What the observer does

- Probes the 8 slots of the current Windows session. Each section is opened with `FILE_MAP_READ`.
- Never acquires the NVIDIA mutex. The mutex is opened with `READ_CONTROL` only, to read its security descriptor.
- Never writes to a section and never calls `attach` or `publish`.
- Reports the owner SID and the DACL (as SDDL) of every section and mutex.
- Optionally records process context: PID, image name and path, creation time and Windows session.
- Distinguishes `absent`, `access_denied`, `open_error`, `map_error`, `insufficient_range`, `unknown_version` and `unstable` results, with the Win32 error code and the stage that failed.

## What it does NOT prove

- `--pid` and `--process` only add observation context. They do not prove that a slot belongs to, or is consumed by, that process.
- Section names do not contain a PID: a name is the fixed stem plus the slot index.
- A counter change is sampled activity, not FPS and not a presented frame.
- Two identical reads do not prove a coherent snapshot. The mutex is deliberately not taken, so a read can race a writer.
- An owner SID does not identify a process, and an ACL does not equal effective permissions.

## Requirements

- Windows 10 or later
- MSVC (Visual Studio 2022 or Build Tools) with the Windows SDK
- CMake 3.20 or later

No third-party dependencies. The optional ImGui glue in `nvovr.hpp` is not built by default.

## Build and test

```text
cmake --preset vs2022
cmake --build --preset debug
ctest --preset debug
```

The tests use in-memory byte buffers and uniquely named synthetic mappings. They never touch the NVIDIA section names and never run `attach` or `publish`.

## Usage

```text
nv_research snapshot [--pid <pid> | --process <name.exe>] [--format text|jsonl]
nv_research watch [--interval-ms 250] [--duration-s 30]
                  [--pid <pid> | --process <name.exe>]
                  [--output <file.jsonl>] [--overwrite]
nv_research security [--format text|jsonl]
```

- `snapshot`: one reading of all 8 slots. Process context is collected only if `--pid` or `--process` is given.
- `watch`: samples periodically and logs the initial state, slot changes, process changes and an end record. Ctrl+C stops it cleanly. An existing output file is not overwritten unless `--overwrite` is given.
- `security`: owner and DACL of each section and mutex.

Slots show up as `absent` (Win32 error 2) when the overlay is not running or uses a different namespace.

### JSONL output

Every record carries `schema`, `kind`, `utc_ms` and `mono_ms`. Kinds are `snapshot`, `slot_change`, `process_change`, `security` and `end`. Records tied to a slot additionally carry `session_id`.

## Library: `nvovr.hpp`

A single-header, STB-style library. Define `nvovr_implementation` in exactly one translation unit before including it:

```cpp
#define nvovr_implementation
#include <nvovr.hpp>
```

It exposes `c_session` (binds a slot and restores its original fields on destruction) and `c_compositor` (owns the D3D11 device and the shared texture). Attaching only opens the named section and mutex and validates the header. It does not demonstrate which process consumes the slot, and it does not involve any driver or kernel code path. Treat it as an experimental backend.

## Naming convention

All project code uses lowercase snake_case: `c_` for classes, `s_` for structs, `m_` for data members, `g_` for globals and namespace-level constants, `e_` for enums and `t_` for aliases and template parameters. Windows, D3D, DXGI, ImGui and STL identifiers keep their original names. See [`docs/naming.md`](docs/naming.md) for the full rules and the migration table from the original names.

## Project layout

```text
nvovr.hpp                     composition library (not used by the observer)
include/nv_research/          slot layout, observer and output headers
src/                          observer implementation, output formatting, CLI
tests/                        observer tests and library smoke tests
docs/                         research protocol and naming conventions
```

## Research protocol

See [`docs/research_protocol.md`](docs/research_protocol.md) for the manual trials and the limits of the evidence they can provide.

## License

Released under the MIT License. See [`LICENSE`](LICENSE).
