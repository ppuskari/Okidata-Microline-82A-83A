[CmdletBinding()]
param(
    [string]$Repository = 'ppuskari/Okidata-Microline-82A-83A',
    [ValidateSet('public','private')]
    [string]$Visibility = 'public'
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $RepoRoot

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'git is required.'
}
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw 'GitHub CLI (gh) is required.'
}

Write-Host 'Checking GitHub authentication...'
& gh auth status
if ($LASTEXITCODE -ne 0) {
    throw 'GitHub CLI is not authenticated.'
}

if (-not (Test-Path '.git')) {
    & git init
    if ($LASTEXITCODE -ne 0) { throw 'git init failed.' }

    & git checkout -b main
    if ($LASTEXITCODE -ne 0) { throw 'git checkout -b main failed.' }

    & git add .
    if ($LASTEXITCODE -ne 0) { throw 'git add failed.' }

    & git commit -m 'Initial OkiGraph I native graphics test generator'
    if ($LASTEXITCODE -ne 0) { throw 'git commit failed.' }
}

Write-Host "Checking for GitHub repository $Repository..."

# PowerShell 5.1 turns native stderr into ErrorRecord objects when
# ErrorActionPreference is Stop.  Use cmd.exe only for this expected-failure
# existence probe so a missing repository does not terminate the script.
$Probe = 'gh repo view "' + $Repository + '" --json nameWithOwner >NUL 2>NUL'
& cmd.exe /d /c $Probe
$RepoExists = ($LASTEXITCODE -eq 0)

if (-not $RepoExists) {
    Write-Host "Creating $Visibility repository $Repository ..."

    $CreateArgs = @(
        'repo', 'create', $Repository,
        "--$Visibility",
        '--source', '.',
        '--remote', 'origin',
        '--push',
        '--description',
        'Okidata MICROLINE 82A/83A firmware provenance, OkiGraph I analysis, and Linux printing tools'
    )

    & gh @CreateArgs
    if ($LASTEXITCODE -ne 0) {
        throw 'gh repo create failed.'
    }
}
else {
    Write-Host "Repository already exists: $Repository"

    $RemoteUrl = & git remote get-url origin 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $RemoteUrl) {
        & git remote add origin "https://github.com/$Repository.git"
        if ($LASTEXITCODE -ne 0) { throw 'git remote add failed.' }
    }

    & git add .
    if ($LASTEXITCODE -ne 0) { throw 'git add failed.' }

    $Pending = & git status --porcelain
    if ($Pending) {
        & git commit -m 'Update OkiGraph I project files'
        if ($LASTEXITCODE -ne 0) { throw 'git commit failed.' }
    }

    & git push -u origin main
    if ($LASTEXITCODE -ne 0) { throw 'git push failed.' }
}

Write-Host ''
Write-Host 'Repository state:'
& git status
if ($LASTEXITCODE -ne 0) { throw 'git status failed.' }

Write-Host ''
& git remote -v

Write-Host ''
Write-Host "Published: https://github.com/$Repository"
