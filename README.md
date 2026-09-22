# AeroDeck GroundView

### V0.01

A heavily developed MSFS 2024 ground-camera and PFD system focused on
stability, diagnostics, graphics compatibility, and real-world usability.

---

## What is AeroDeck GroundView?

AeroDeck GroundView is an actively developed Microsoft Flight Simulator
2024 camera system derived from the Taxi Cam development project.

The project focuses on providing ground-camera functionality while improving
stability, diagnostics, graphics compatibility, recovery behaviour and
troubleshooting.

The current product is substantially reorganised and redesigned compared
with the original Taxi Cam presentation.

---

## Current Version

**V0.01**

The project version remains V0.01 unless explicitly changed by the developer.

---

## What does it do?

AeroDeck GroundView provides functionality including:

- Ground camera operation
- Aircraft-specific camera profiles
- A350 camera/PFD functionality
- Native simulator integration
- D3D12 graphics capture
- PFD target detection
- Camera lifecycle management
- Camera recovery
- Graphics-state monitoring
- Runtime diagnostics
- Crash evidence collection
- Configurable logging
- Diagnostic log export
- GitHub bug-report integration

---

# Diagnostics & Troubleshooting

A major focus of AeroDeck GroundView is making problems easier to
identify and investigate.

The diagnostic system can record information relating to:

- Simulator state
- Aircraft state
- Camera state
- Graphics state
- PFD state
- Capture activity
- Rendering activity
- Hook failures
- D3D12 queue contention
- Presentation safety state
- Runtime errors
- Crash evidence

Logs can be stored in a user-selected location.

The application can also package available diagnostic information into
a ZIP file for troubleshooting.

---

# Graphics Compatibility

AeroDeck GroundView includes safeguards designed to reduce conflicts
between its graphics pipeline and other graphics systems.

Development and testing includes compatibility considerations for:

- NVIDIA DLSS
- Frame Generation
- Multi Frame Generation where applicable
- ReShade
- Other software interacting with the MSFS D3D12 presentation pipeline

The graphics safety system can temporarily suspend new PFD/capture
injection when graphics queue contention is detected while preserving
the existing camera/resource state.

This is intended to reduce the possibility of AeroDeck becoming involved
in graphics synchronization problems.

Actual compatibility depends on the simulator, GPU driver, MSFS version,
aircraft, graphics configuration and other installed software.

---

# Bug Reports

If you encounter a problem with AeroDeck GroundView, please report it
through the GitHub Issues page:

https://github.com/JoelScripts/A350-Improvements/issues

Please include as much information as possible.

### Please provide:

- AeroDeck GroundView version
- Aircraft being used
- MSFS version
- What you were doing immediately before the problem
- Whether the camera system was active
- Whether the PFD system was active
- Whether DLSS was enabled
- Whether Frame Generation was enabled
- Whether ReShade was installed/enabled
- Any visible error messages
- Screenshots or video where useful
- The exported AeroDeck diagnostic ZIP where possible

### Diagnostic ZIPs are especially useful

If AeroDeck GroundView generated diagnostic or crash evidence,
please include the exported ZIP when reporting a problem.

This can significantly reduce the amount of information that has to be
recreated manually.

---

# Development Status

AeroDeck GroundView is actively developed and tested against real-world
MSFS 2024 usage.

Some functionality may remain experimental or require testing against
specific aircraft, graphics configurations or simulator versions.

A feature being present in the application does not necessarily mean
that compatibility with every MSFS configuration has been verified.

---

# Project History

AeroDeck GroundView originated from the Taxi Cam development work.

The project began as **TaxiCam-Fixed**, an improved development version
of the existing Taxi Cam implementation.

The project has subsequently undergone substantial changes to its:

- Source organisation
- Runtime structure
- Module organisation
- User interface
- Branding
- Diagnostics
- Logging
- Graphics safety systems
- Configuration systems
- Build system
- Installer structure

The current project is therefore substantially different in presentation
and development structure from the original Taxi Cam project while
retaining the underlying functionality required for the simulator
integration.

---

# Original Taxi Cam Project

The original Taxi Cam project can be found here:

https://github.com/rthoms334/taxi-cam

Please refer to the original project for its original implementation,
licensing and attribution information.

AeroDeck GroundView does not claim authorship of the original Taxi Cam
implementation.

---

# Development Philosophy

The development priorities are:

1. Preserve working functionality
2. Maintain buildability
3. Improve stability
4. Improve diagnostics
5. Improve graphics compatibility
6. Improve recovery behaviour
7. Improve the user experience

Changes are tested locally before being considered ready.

---

# Developer

**Joel**

Project:

https://github.com/JoelScripts/A350-Improvements

Bug reports:

https://github.com/JoelScripts/A350-Improvements/issues
