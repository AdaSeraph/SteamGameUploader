[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    # Also create dist\SteamGameUploader-<Version>-win-x64.zip and a matching .sha256 file.
    [switch]$Package,
    [ValidatePattern('^[0-9A-Za-z._-]+$')]
    [string]$Version = 'dev'
)

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'vswhere.exe was not found. Install Visual Studio 2022 or later with the "Desktop development with C++" workload.'
}

$install = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ([string]::IsNullOrWhiteSpace($install)) {
    throw 'No Visual Studio installation with MSBuild and the x64 C++ toolset was found.'
}

$msbuild = Join-Path $install 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild (Join-Path $PSScriptRoot 'SteamUploader.sln') /m /nologo /v:minimal "/p:Configuration=$Configuration" /p:Platform=x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $PSScriptRoot "bin\$Configuration\SteamUploader.exe"
Write-Host "Build succeeded: $exe" -ForegroundColor Green

if ($Package) {
    $name = "SteamGameUploader-$Version-win-x64"
    $dist = Join-Path $PSScriptRoot 'dist'
    $staging = Join-Path $dist $name
    $zip = Join-Path $dist "$name.zip"
    foreach ($old in @($staging, $zip, "$zip.sha256")) {
        if (Test-Path -LiteralPath $old) { Remove-Item -LiteralPath $old -Recurse -Force }
    }
    New-Item -ItemType Directory -Path $staging -Force | Out-Null
    $files = @($exe) + (@('README.md', 'README.zh-CN.md', 'LICENSE') | ForEach-Object { Join-Path $PSScriptRoot $_ })
    Copy-Item -LiteralPath $files -Destination $staging
    Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $zip
    $hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath "$zip.sha256" -Value "$hash  $name.zip" -Encoding Ascii
    Write-Host "Package: $zip" -ForegroundColor Green
}
