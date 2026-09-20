# Taxi Cam Fixed 0.9.45

## Diagnostics file access and Issue #1 investigation

- Keeps the 0.9.44 external camera transition guard unchanged.
- Fixes **Open log folder** so it opens `%LOCALAPPDATA%\Taxi Cam`, rather than the general Taxi Cam settings directory.
- Makes every diagnostics file button initialize the diagnostics paths defensively before opening.
- If a diagnostic snapshot file is missing, the companion performs a bootstrap state write and checks again.
- Adds an Explorer `/select` fallback when Windows has no file association for a diagnostics file such as `.jsonl`.
- Records `diagnostics.folder_opened`, `diagnostics.folder_open_failed`, `diagnostics.file_opened`, and `diagnostics.file_open_failed` events.
- Keeps `diagnostics-summary.txt` and `advanced-state.txt` as current-state files.
- Keeps `advanced-diagnostics.log` and `advanced-diagnostics.jsonl` as append-only event histories.
- This remains an investigation build; it does not claim Issue #1 is fixed until reproduced and retested in MSFS 2024.
