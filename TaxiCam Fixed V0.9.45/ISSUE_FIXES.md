# TaxiCam-Fixed 0.9.29 — Issue Fix Candidate

This tree starts from the supplied upstream `taxi-cam-main(3).zip` baseline and preserves its PFD/D3D12/camera implementation.

## Stability changes in this candidate

- Capture watchdog now recovers from sustained zero-frame progress even when the graphics layer does not classify the source state as `unknown`.
- Recovery is rate-limited to avoid repeatedly touching live GPU camera resources.
- A successful new frame immediately clears the watchdog stall state.
- Scene recovery permits more transient retries and uses a shorter retry interval, improving aircraft-load and post-landing recovery without changing the native camera ownership model.
- The existing ReShade native-device identity checks, non-blocking command-queue contention path, PFD target discovery, GPU-generation handling, scene-session reset and camera retirement mechanisms from the supplied baseline are retained.

## Upstream reports targeted

#71 ReShade crash/static output — retain proxy-safe native-device validation and improve stalled-frame recovery.
#69 connection freeze — retain fail-open queue submission behavior; avoid adding blocking recovery work.
#62 ready camera pair / zero captured frames — zero-progress watchdog now requests a bounded source-state rearm.
#57 waiting for cockpit displays — retain repeated PFD discovery and live inventory refresh.
#56 AMD verified-GPU-state stall — watchdog rearm is now independent of an `unknown_source_state` classification.
#54 post-landing failure — scene recovery has a larger transient retry budget and faster retry.
#53 simulator freeze — preserve non-blocking queue contention and GPU/device guards.
#52 A350-1000 no output — preserve the dedicated A35K profile and PFD target policy.
#49 graphics-device failure — preserve device-removed checks and quarantine behavior.
#48 verified-GPU-state stall — same bounded watchdog/rearm path as #56.

## Verification status

This is a source-level fix candidate. The simulator, GPU drivers, ReShade/DLSS/Frame Generation combinations and aircraft cannot be executed in this build environment, so the issues are not claimed as independently hardware-verified here.

Recommended first test: build this tree, load the iniBuilds A350-1000, activate TAXI, and confirm that live PFD output still works. Then test a landing/re-activation cycle and, separately, ReShade/DLSS/Frame Generation.


## 0.9.30 diagnostics upgrade

- Added structured `%LOCALAPPDATA%\TaxiCam-Fixed\diagnostics.log`.
- Added module/severity/error-code/source-file/function/line fields.
- Bridge status events now carry their actual call-site source location.
- Added an explicit D3D12 initialization failure event.
- Diagnostics distinguish observed failure locations from root-cause claims.
- Diagnostics are best-effort and are designed never to interrupt simulator operation.


## 0.9.31 — Detailed diagnostics layer

- Added session-correlated human-readable diagnostic events.
- Added companion watchdog health snapshots and stale-heartbeat/recovery events.
- Added observable Taxi intent transition events without claiming a specific external input source.
- Added pipeline progress events for capture/stamp changes.
- Preserved the existing low-level bridge diagnostics and failure evidence.
- Diagnostics remain observational and do not assert root cause from timing alone.


## 0.9.32 — GitHub Bug Reports UI
- Added a read-only GitHub bug-report browser to the companion UI.
- Uses the public `JoelScripts/A350-Improvements` Issues API endpoint.
- Filters pull requests because GitHub's Issues API can return both issues and pull requests.
- Added refresh/open/report actions without embedding GitHub write credentials.

## 0.9.39

- Fixed the unused `W` local in `src/app/companion.cpp` that caused the Windows companion build to fail under `-Werror`.
- Preserved the 0.9.38 fresh UI redesign and existing features.


## 0.9.41 — Approved fresh UI
- Retains the fresh-canvas UI layout from 0.9.40.
- No TaxiCam camera/PFD engine changes.
