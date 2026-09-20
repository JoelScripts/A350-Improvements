Set-StrictMode -Version Latest

function Get-TaxiReleaseBuildNumber(
    [string]$GitHubActions = $env:GITHUB_ACTIONS,
    [string]$EventName = $env:GITHUB_EVENT_NAME,
    [string]$Ref = $env:GITHUB_REF,
    [string]$RunNumber = $env:GITHUB_RUN_NUMBER
) {
    # Local builds never require Git and use build number 0.
    # CI main-branch builds may use the workflow run number when available.
    if ($GitHubActions -cne 'true') { return 0 }
    if ($EventName -like 'pull_request*') { return 0 }
    if ($Ref -cne 'refs/heads/main') { return 0 }
    if ([string]::IsNullOrWhiteSpace($RunNumber)) { return 0 }

    $buildNumber = 0
    if ($RunNumber -notmatch '^[1-9][0-9]{0,9}$' -or
        -not [int]::TryParse($RunNumber, [ref]$buildNumber)) {
        throw 'Invalid GitHub build number.'
    }
    return $buildNumber
}

function Get-TaxiVersion([string]$Repository, [int]$BuildNumber = 0) {
    if ($BuildNumber -lt 0) { throw 'Build number must not be negative.' }

    $path = Join-Path $Repository 'version.json'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw 'version.json is required.'
    }

    $config = Get-Content -Raw -LiteralPath $path | ConvertFrom-Json

    # Accept both the newer uppercase schema and the original lowercase schema.
    $rawVersion = $null
    if ($config.PSObject.Properties.Name -contains 'Version') {
        $rawVersion = [string]$config.Version
    } elseif ($config.PSObject.Properties.Name -contains 'version') {
        $rawVersion = [string]$config.version
    }

    if ([string]::IsNullOrWhiteSpace($rawVersion) -or
        $rawVersion -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
        throw 'version.json must contain a major.minor.patch version.'
    }

    $parts = @($Matches[1], $Matches[2], $Matches[3])
    foreach ($part in $parts) {
        $number = 0
        if (-not [int]::TryParse($part, [ref]$number) -or $number -gt 65535) {
            throw 'Windows version components must fit in 16 bits.'
        }
    }

    [pscustomobject]@{
        Version = "$($parts[0]).$($parts[1]).$($parts[2])"
        Major = [int]$parts[0]
        Minor = [int]$parts[1]
        Patch = [int]$parts[2]
        BuildNumber = $BuildNumber
        BaseCommit = ''
        CommitsSinceBase = 0
    }
}
