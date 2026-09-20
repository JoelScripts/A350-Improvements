# TaxiCam-Fixed 0.9.29 — Standalone Local Build

This package is a local development build of Taxi Cam with the issue-fix candidate changes.

## Local build requirements

- Windows PowerShell
- `clang++.exe` on PATH
- `llvm-windres.exe` on PATH

## Git/GitHub

The local build does **not** require:
- a Git repository
- a `.git` directory
- GitHub
- GitHub Actions
- `dependencies.json` for compiler discovery
- downloading a compiler with `bootstrap.ps1`

## Build

From the `taxi-cam-main` directory:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\build.ps1
```

The build produces the compiled bridge and companion executable under:

`build\native\`

The source tree remains the thing you edit; the DLL/EXE are rebuilt outputs used for MSFS testing.
