# TaxiCam-Fixed 0.9.44 — Issue Investigation

## GitHub issue #1: external camera / ChasePlane view-change freeze

This build adds a bounded camera-transition guard specifically to investigate the first open issue in the JoelScripts/A350-Improvements repository.

### What changes during an external camera transition

1. The public SimConnect camera sample is observed from the existing telemetry worker cache.
2. A large instantaneous position/target/orientation/FOV jump is treated as an external camera transition candidate.
3. Taxi Cam rendering is temporarily suspended.
4. The active PFD target mask is temporarily closed.
5. Existing Taxi Cam camera resources are retained; no camera destruction/recreation is performed by the guard.
6. The guard waits for 150 ms of public-camera stability.
7. If stability is not observed, the guard has a hard 1.5 second timeout and resumes anyway.
8. Advanced Diagnostics records transition begin/end/timeout/cancel events.

### What this does not claim

This is an investigative safety change. It does not prove that the GitHub issue is fixed until it is reproduced in MSFS 2024 with the same external camera workflow and the resulting diagnostics are reviewed.
