param(
    [string]$Compiler = '',
    [string]$OutputDirectory = '',
    [string]$RomPath = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'native_process.ps1')
$root = Split-Path $PSScriptRoot -Parent
if (-not $Compiler) {
    if ($env:GBDK_HOME) { $Compiler = Join-Path $env:GBDK_HOME 'bin/lcc.exe' }
    else { $Compiler = (Get-Command lcc.exe -ErrorAction Stop).Source }
}
$Compiler = (Resolve-Path -LiteralPath $Compiler).Path
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root '.build' }
if (-not $RomPath) { $RomPath = Join-Path $root 'build/DigiTCGB.gbc' }
New-Item -ItemType Directory -Force -Path $OutputDirectory,(Split-Path $RomPath -Parent) | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$RomPath = [IO.Path]::GetFullPath($RomPath)
$oldMode = [DigiNative.Errors]::GetErrorMode()
$mounts = @()
$pushed = $false
try {
    [void][DigiNative.Errors]::SetErrorMode($oldMode -bor 0x8003)
    & (Join-Path $PSScriptRoot 'generate_cards.ps1')
    foreach ($path in @($root,$OutputDirectory,(Split-Path (Split-Path $Compiler -Parent) -Parent))) {
        $letter = @('Z','Y','X','W','V','U','T','S','R','Q') | Where-Object { -not (Test-Path ($_ + ':\')) -and -not (Get-PSDrive -Name $_ -ErrorAction SilentlyContinue) } | Select-Object -First 1
        if (-not $letter) { throw 'No unused drive letter is available' }
        & subst.exe ($letter + ':') $path
        if ($LASTEXITCODE -ne 0) { throw 'Could not create a temporary build path' }
        $mounts += $letter
    }
    Push-Location ($mounts[0] + ':\')
    $pushed = $true
    $output = $mounts[1] + ':/'
    $Compiler = $mounts[2] + ':/bin/' + (Split-Path $Compiler -Leaf)
    $sources = @('core/cards','core/game','core/effects','core/opponent','gbc/audio','gbc/music_data','gbc/art_upload','gbc/art_tiles_13','gbc/portraits_6','gbc/portraits_7','gbc/portraits_8','gbc/portraits_9','gbc/portraits_10','gbc/portraits_11','gbc/portraits_12','gbc/gfx','gbc/ui_data','gbc/ui_draw','gbc/ui','gbc/main')
    $objects = @()
    foreach ($source in $sources) {
        $obj = $output + ($source -replace '/', '_') + '.o'
        Write-Output ('Compiling ' + $source)
        & $Compiler -Wf--max-allocs-per-node3000 -Isrc/core -Isrc/gbc -c -o $obj ('src/' + $source + '.c')
        if ($LASTEXITCODE -ne 0) { throw ('Compilation failed: ' + $source) }
        $objects += $obj
    }
    & $Compiler -Wl-m -Wl-j -Wm-yC -Wm-yt0x19 -Wm-yo32 -Wm-ynDIGITCGB -o ($output + 'DigiTCGB.gbc') @objects
    if ($LASTEXITCODE -ne 0) { throw 'ROM link failed' }
    Copy-Item -LiteralPath (Join-Path $OutputDirectory 'DigiTCGB.gbc') -Destination $RomPath
    Write-Output 'Built DigiTCGB.gbc (512 KiB, Game Boy Color, MBC5)'
} finally {
    if ($pushed) { Pop-Location }
    foreach ($letter in $mounts) { & subst.exe ($letter + ':') /D }
    [void][DigiNative.Errors]::SetErrorMode($oldMode)
}
