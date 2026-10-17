# Research protocol

## Goal

Empirically characterize the behavior of the NVIDIA overlay slots and the trust boundaries of its IPC, without modifying anything.

## Metadata to record

Record these by hand when they cannot be detected reliably:

- Windows, NVIDIA App and driver versions.
- Applications used (your own or test applications only) and the overlay state.

## Trials

1. **Baseline**: run `snapshot` with the overlay disabled and then enabled, with no application open.
2. **Single application**: run `watch --output run1.jsonl` while opening and closing one of your own applications.
3. **Two simultaneous applications**: open both and observe which slots change.
4. **Repetition**: repeat each trial at least three times and compare the results.
5. **Security**: run `security` before and during a trial and compare owner and DACL.

## Limits of the evidence

- If a slot and a process change at the same time, that is a **temporal correlation**, not proof that the process consumes the slot.
- The owner SID of a section does not identify a PID.
- An ACL is not the same as effective permissions.
- Without the mutex, reads can be incoherent. The `unstable` status means two consecutive reads did not match.
- Proving an unambiguous association requires evidence that is outside the scope of this first version.

## Out of scope

Graphics capture, texture publishing, key recovery, reading another process's memory, drivers, elevation and global handle enumeration.
