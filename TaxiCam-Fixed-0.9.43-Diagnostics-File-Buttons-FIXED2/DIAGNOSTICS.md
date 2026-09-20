# TaxiCam-Fixed 0.9.30 — Diagnostics Upgrade

This build preserves the working Taxi Cam runtime and adds structured runtime diagnostics.

## Log files

The new structured diagnostic log is:

`%LOCALAPPDATA%\TaxiCam-Fixed\diagnostics.log`

The existing bridge telemetry remains:

`%LOCALAPPDATA%\Taxi Cam\bridge.log`

## What the structured log records

Each diagnostic record contains:

- UTC timestamp
- process ID
- thread ID
- module
- severity
- diagnostic error/event code
- source file
- function
- source line
- diagnostic message

Example:

```text
module=[TaxiCam-Fixed::D3D12] level=ERROR code=D3D12-INIT-FAILED
file=bridge_main.cpp function=run_impl line=...
```

## Evidence rules

The logger deliberately distinguishes **observed location** from **root cause**.

A source file is not called the root cause merely because it appears in a log entry. It is the location where the event was observed or emitted.

Future issue fixes should use this order:

1. Identify the first failed pipeline stage.
2. Record the source location where that failure is observed.
3. Record the upstream/downstream operation chain.
4. Only then nominate investigation candidates.
5. Confirm the candidate by reproducing the failure after the source change.

This prevents diagnostics from turning a guess into a false "likely source" claim.

## Build

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\build.ps1
```


## 0.9.31 diagnostics additions

Version 0.9.31 keeps the existing low-level diagnostics and adds a companion-side evidence layer.

- `WATCH-SESSION-START`: starts a diagnostics correlation session.
- `CAMERA-INTENT-CHANGED`: records an observable Taxi intent/camera activation-mask transition. This does not claim that a particular external camera key was pressed.
- `PIPELINE-PROGRESS`: records observed capture/stamp progress.
- `WATCHDOG-SNAPSHOT`: periodic human-readable health snapshot.
- `WATCHDOG-STALE`: records that the bridge heartbeat became stale.
- `WATCHDOG-RECOVERED`: records recovery after a previously stale heartbeat.

Each event includes a session identifier, classification, source file/function/line and an event code. Classifications are evidence labels, not root-cause conclusions.

The watchdog is intentionally observational: it records stale heartbeats and existing recovery behaviour but does not declare TaxiCam the cause of an MSFS freeze or crash. Windows Event ID 1000 is the actual application crash event when Windows records an application crash; Event ID 1001 contains Windows Error Reporting information and can be used as supporting correlation data.
