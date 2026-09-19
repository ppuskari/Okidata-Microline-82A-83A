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

& gh auth status
if ($LASTEXITCODE -ne 0) {
    throw 'GitHub CLI is not authenticated.'
}

if (-not (Test-Path '.git')) {
    & git init -b main
    if ($LASTEXITCODE -ne 0) { throw 'git init failed.' }

    & git add .
    if ($LASTEXITCODE -ne 0) { throw 'git add failed.' }

    & git commit -m 'Initial OkiGraph I native graphics test generator'
    if ($LASTEXITCODE -ne 0) { throw 'git commit failed.' }
}

$null = & gh repo view $Repository --json nameWithOwner 2>$null
if ($LASTEXITCODE -ne 0) {
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
} else {
    Write-Host "Repository already exists: $Repository"

    $null = & git remote get-url origin 2>$null
    if ($LASTEXITCODE -ne 0) {
        & git remote add origin "https://github.com/$Repository.git"
        if ($LASTEXITCODE -ne 0) { throw 'git remote add failed.' }
    }

    & git add .
    if ($LASTEXITCODE -ne 0) { throw 'git add failed.' }

    $pending = & git status --porcelain
    if ($pending) {
        & git commit -m 'Update OkiGraph I native graphics test generator'
        if ($LASTEXITCODE -ne 0) { throw 'git commit failed.' }
    }

    & git push -u origin main
    if ($LASTEXITCODE -ne 0) { throw 'git push failed.' }
}

Write-Host "Published: https://github.com/$Repository"
