<#
.SYNOPSIS
    Build (if needed) and start the handme server on Windows.

.DESCRIPTION
    Windows counterpart of start.sh: compiles src/*.c into build\handme.exe when
    the binary is missing or older than the sources, then runs the server in the
    foreground.

    A C toolchain is required. The first one found is used:
      1. cl.exe (MSVC) already on PATH
      2. gcc.exe (MinGW-w64) on PATH
      3. MSVC from Visual Studio / Build Tools, located through vswhere; the
         compiler environment is imported automatically
      4. clang.exe with a MinGW target

    Set the PORT environment variable to listen on a port other than 8080.

.PARAMETER ServerArgument
    Arguments passed through to build\handme.exe.

.EXAMPLE
    .\start.ps1

.EXAMPLE
    $env:PORT = 9000
    .\start.ps1
#>
[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $ServerArgument
)

$ErrorActionPreference = 'Stop'

if (-not $ServerArgument) { $ServerArgument = @() }

$root = $PSScriptRoot
if (-not $root) { $root = (Get-Location).Path }
$buildDir = Join-Path $root 'build'
$exe = Join-Path $buildDir 'handme.exe'
$includeDir = Join-Path $root 'include'
$sources = @((Join-Path $root 'src\main.c'), (Join-Path $root 'src\mongoose.c'))

function Test-RebuildNeeded {
    param([string] $Binary, [string[]] $Inputs)

    if (-not (Test-Path -LiteralPath $Binary)) { return $true }
    $stamp = (Get-Item -LiteralPath $Binary).LastWriteTimeUtc
    foreach ($inputPath in $Inputs) {
        if (-not (Test-Path -LiteralPath $inputPath)) { continue }
        if ((Get-Item -LiteralPath $inputPath).LastWriteTimeUtc -gt $stamp) {
            return $true
        }
    }
    return $false
}

function Get-Executable {
    param([string[]] $Name)

    foreach ($candidate in $Name) {
        $command = Get-Command -Name $candidate -CommandType Application `
            -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
    }
    return $null
}

# Imports the environment of the newest Visual Studio C++ toolset (cl.exe,
# INCLUDE, LIB, ...) into this process. Returns $true on success.
function Import-MsvcEnvironment {
    $programFilesX86 = ${env:ProgramFiles(x86)}
    if (-not $programFilesX86) { return $false }

    $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { return $false }

    $installationPath = & $vswhere -latest -products '*' `
        -requires 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64' `
        -property installationPath 2>$null | Select-Object -First 1
    if (-not $installationPath) { return $false }

    $vcvars = Join-Path $installationPath.Trim() 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path -LiteralPath $vcvars)) { return $false }

    # Run vcvars64.bat through a temporary batch file so that paths containing
    # spaces do not have to survive several layers of quoting.
    $batch = Join-Path ([System.IO.Path]::GetTempPath()) `
        ('handme-vcvars-' + [guid]::NewGuid().ToString('N') + '.bat')
    $lines = $null
    try {
        Set-Content -LiteralPath $batch -Encoding ASCII -Value @(
            ('@call "' + $vcvars + '" >nul 2>&1'),
            '@set'
        )
        $lines = & cmd.exe /c $batch
        if ($LASTEXITCODE -ne 0) { return $false }
    } finally {
        Remove-Item -LiteralPath $batch -Force -ErrorAction SilentlyContinue
    }

    foreach ($line in $lines) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path ('env:' + $Matches[1]) -Value $Matches[2]
        }
    }
    return $true
}

$inputs = @($sources)
$headers = Get-ChildItem -LiteralPath @((Join-Path $root 'src'), $includeDir) `
    -Filter '*.h' -File -Recurse -ErrorAction SilentlyContinue
if ($headers) { $inputs += $headers | ForEach-Object { $_.FullName } }

if (Test-RebuildNeeded -Binary $exe -Inputs $inputs) {
    $cl = Get-Executable @('cl.exe')
    $cc = $null
    $mode = $null
    $target = $null

    if ($cl) {
        $mode = 'msvc'
    } else {
        $cc = Get-Executable @('gcc.exe')
        if ($cc) {
            $mode = 'gnu'
        } elseif (Import-MsvcEnvironment) {
            $cl = Get-Executable @('cl.exe')
            if ($cl) { $mode = 'msvc' }
        }
        if (-not $mode) {
            $cc = Get-Executable @('clang.exe')
            if ($cc) {
                $target = & $cc -dumpmachine 2>$null | Select-Object -First 1
                if ($target -match 'msvc') {
                    throw ('clang.exe targets MSVC but no MSVC linker was found. ' +
                        'Install the Visual Studio Build Tools with the C++ workload, ' +
                        'or install MinGW-w64, and run this script again.')
                }
                $mode = 'gnu'
            }
        }
    }

    if (-not $mode) {
        $hint = @(
            'No C compiler found. Install MinGW-w64 GCC:',
            '  winget install --id BrechtSanders.WinLibs.POSIX.UCRT --exact',
            'or the Visual Studio C++ Build Tools, then open a new terminal.'
        ) -join [Environment]::NewLine
        throw $hint
    }

    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

    if ($mode -eq 'msvc') {
        Write-Host 'Building handme with MSVC...'
        Push-Location -LiteralPath $buildDir
        try {
            & $cl /nologo /O2 /W3 ('/I' + $includeDir) $sources `
                ('/Fe:' + $exe) /link ws2_32.lib
            $code = $LASTEXITCODE
        } finally {
            Pop-Location
        }
    } else {
        Write-Host 'Building handme with GCC/Clang...'
        & $cc -std=gnu11 -O2 -Wall -Wextra ('-I' + $includeDir) $sources `
            -o $exe -lws2_32
        $code = $LASTEXITCODE
    }

    if ($code -ne 0) {
        throw "Build failed (compiler exit code $code)."
    }
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Build finished but $exe was not created."
    }
}

Push-Location -LiteralPath $root
$code = 0
try {
    & $exe @ServerArgument
    $code = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $code
