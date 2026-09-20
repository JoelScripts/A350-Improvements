# TaxiCam-Fixed 0.9.44

## Camera transition safety / Issue #1 investigation

- Detects large external public-camera changes without changing the simulator camera.
- Temporarily pauses Taxi Cam rendering and PFD target gates during a detected external camera transition.
- Retains existing Taxi Cam camera resources instead of destroying/recreating them.
- Resumes automatically after camera stability, with a bounded 1.5 second timeout.
- Adds explicit advanced diagnostic events for transition begin/end/timeout/cancel.
- Makes `diagnostics-summary.txt` and `advanced-state.txt` true current-state files instead of continually appending snapshots.
- Preserves the existing capture/PFD/D3D12 implementation.

This build is intended specifically to reproduce and investigate the open ChasePlane/external-view freeze report. It does not claim the issue is fixed until reproduced and retested in MSFS 2024.
