# TaxiCam-Fixed 0.9.45 Build 2

## Compile fix

Build 1 failed in `src/bridge/bridge_main.cpp` because `CameraTransitionGuard` is declared in `taxi_camera::standalone`, while `bridge_main.cpp` only imports the parent `taxi_camera` namespace.

The declaration is now explicitly qualified as:

`standalone::CameraTransitionGuard camera_transition_guard;`

No camera-transition logic, diagnostics behavior, PFD/D3D12 path, or file-access changes were altered.

This remains a source build; it has not been compiled with Windows/MSVC in this environment.


## Build 3
- Fixed MSVC/Clang compile error in `src/app/companion.cpp` caused by `reinterpret_cast<INT_PTR>(0)` in the diagnostics-folder button handler.
- Replaced the invalid cast with an explicit `INT_PTR result = 0` and conditional `ShellExecuteW` call.
