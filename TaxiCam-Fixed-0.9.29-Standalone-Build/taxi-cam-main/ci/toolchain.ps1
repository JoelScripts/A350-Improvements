function Get-TaxiToolchain([string]$Repository) {
    $clang = Get-Command clang++.exe -ErrorAction SilentlyContinue
    if (-not $clang) {
        throw 'clang++.exe was not found on PATH. Install LLVM/MinGW and ensure clang++.exe is available in PATH.'
    }

    $windres = Get-Command llvm-windres.exe -ErrorAction SilentlyContinue
    if (-not $windres) {
        throw 'llvm-windres.exe was not found on PATH. Install LLVM/MinGW and ensure llvm-windres.exe is available in PATH.'
    }

    return Split-Path -Parent $clang.Source
}

function Write-TaxiLlvmConfig([string]$Repository, [string]$OutputDirectory) {
    $toolchain = Get-TaxiToolchain $Repository
    $libraries = @(Get-ChildItem -LiteralPath $toolchain -Filter 'libLLVM-*.dll' -File -ErrorAction SilentlyContinue)
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

    if ($libraries.Count -eq 1) {
        $library = $libraries[0].FullName.Replace('\', '/').Replace('"', '\"')
        @('#pragma once', ('#define TAXI_LLVM_LIBRARY_PATH L"' + $library + '"')) |
            Set-Content -LiteralPath (Join-Path $OutputDirectory 'llvm_library.hpp') -Encoding utf8
    } else {
        @('#pragma once', '#define TAXI_LLVM_LIBRARY_PATH L""') |
            Set-Content -LiteralPath (Join-Path $OutputDirectory 'llvm_library.hpp') -Encoding utf8
    }
}
