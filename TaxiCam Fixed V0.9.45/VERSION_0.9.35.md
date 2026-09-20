# TaxiCam-Fixed 0.9.35

## Build corrections

- Fixed the diagnostics bridge build error caused by calling the rotating logger without its `taxi_camera::standalone` namespace qualification.
- Retained the bridge diagnostics macro fix from 0.9.33.
- Retained the detailed diagnostics pipeline introduced for 0.9.31.
- Retained the GitHub Bug Reports tab introduced for 0.9.32.
- Retained the existing installer/update project for producing a user-facing Windows Setup EXE.

## Installer direction

The intended release artifact is a compiled `taxi-cam-0.9.35-windows-x64-setup.exe`. End users should receive the compiled installer rather than being asked to run the development PowerShell build workflow.
