$ErrorActionPreference = 'Stop'

$buildToolsEnv = 'C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$projectRoot = Split-Path -Parent $PSScriptRoot
$lvglPath = Join-Path $projectRoot 'external\lvgl'
$patchPath = Join-Path $projectRoot 'patches\lvgl-animatedgif-max-width-648.patch'
$expectedCommit = '85aa60d18b3d5e5588d7b247abf90198f07c8a63'

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git is required but was not found on PATH.'
}

if (-not (Test-Path -LiteralPath $buildToolsEnv)) {
    winget install --source winget --exact --id Microsoft.VisualStudio.2022.BuildTools `
        --accept-source-agreements --accept-package-agreements --silent --disable-interactivity `
        --override '--wait --quiet --norestart --nocache --installPath C:\BuildTools --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
}

if (-not (Test-Path -LiteralPath (Join-Path $lvglPath 'CMakeLists.txt'))) {
    New-Item -ItemType Directory -Force (Split-Path -Parent $lvglPath) | Out-Null
    git clone --depth 1 --branch v9.5.0 --filter=blob:none https://github.com/lvgl/lvgl.git $lvglPath
    if ($LASTEXITCODE -ne 0) { throw 'LVGL clone failed.' }
}

$actualCommit = (git -C $lvglPath rev-parse HEAD).Trim()
if ($actualCommit -ne $expectedCommit) {
    throw "Unexpected LVGL revision $actualCommit. Expected v9.5.0 commit $expectedCommit."
}

if (-not (Test-Path -LiteralPath $patchPath)) {
    throw "Required LVGL patch is missing: $patchPath"
}

# Reverse-check first makes the 648 px GIF-width patch safe to repeat.
git -C $lvglPath apply --reverse --check -- $patchPath 2>$null
$patchAlreadyApplied = ($LASTEXITCODE -eq 0)
if (-not $patchAlreadyApplied) {
    git -C $lvglPath apply --check -- $patchPath
    if ($LASTEXITCODE -ne 0) {
        throw 'The LVGL GIF-width patch does not apply cleanly.'
    }
    git -C $lvglPath apply -- $patchPath
    if ($LASTEXITCODE -ne 0) { throw 'Applying the LVGL GIF-width patch failed.' }
}

Write-Host 'Bootstrap complete: VS Build Tools ready; LVGL v9.5.0 verified; 648 px GIF patch present.' -ForegroundColor Green
