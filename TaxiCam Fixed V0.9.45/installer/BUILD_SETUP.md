# Building the TaxiCam-Fixed Windows Installer

The repository contains an Inno Setup definition and installer build pipeline.
The end-user release should contain a compiled `taxi-cam-<version>-windows-x64-setup.exe` so users do not need PowerShell or developer tools.

## Developer/release build

1. Build and validate the native binaries with the normal root `build.ps1` workflow.
2. Produce the versioned Windows x64 runtime ZIP expected by `installer/build.ps1`.
3. Run `installer/build.ps1 -Package <versioned-build-zip>`.
4. The installer is written to `build/packages/`.

The installer uses Inno Setup and the existing installer scripts. The first-party Inno Setup download is available from:
https://jrsoftware.org/isdl.php

The installer preserves the existing application identity and is designed to update an existing installation while retaining user settings and diagnostic history unless the user explicitly chooses to remove saved settings.

## Important

This source archive contains the installer project, not a precompiled Windows Setup EXE. A Setup EXE must be compiled on Windows with the native TaxiCam payload and validated release artifacts.
