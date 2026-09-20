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
