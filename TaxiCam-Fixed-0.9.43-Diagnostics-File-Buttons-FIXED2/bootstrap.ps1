[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Write-Output 'TaxiCam-Fixed standalone build: no Git, GitHub checkout, or compiler bootstrap is required.'
Write-Output 'Ensure clang++.exe and llvm-windres.exe are available on PATH, then run .\build.ps1.'
