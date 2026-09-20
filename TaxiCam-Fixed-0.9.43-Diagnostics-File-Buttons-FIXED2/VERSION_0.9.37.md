# TaxiCam-Fixed 0.9.37

UI redesign build with Windows companion compile fixes.

Fixes carried from 0.9.36:
- Restored the missing `connection_worker` implementation.
- Added the missing `draw_page` line helper used by the redesigned footer.
- Removed the unused GitHub issue timestamp state.

The 0.9.36 UI redesign and existing Taxi Cam functionality are retained.

Compile fix: corrected the footer separator draw_page() call to supply the required Y2 coordinate to the local line() helper.
