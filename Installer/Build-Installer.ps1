[CmdletBinding()]
param(
    [string]$PackageRoot = (Join-Path $PSScriptRoot '..\package'),
    [string]$OutputRoot = (Join-Path $PSScriptRoot '..\installer-package')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repository = Split-Path $PSScriptRoot -Parent
$versionMatch = [regex]::Match((Get-Content (Join-Path $repository 'CMakeLists.txt') -Raw), 'project\(Sila VERSION ([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $versionMatch.Success) { throw 'Cannot read Sila version from CMakeLists.txt' }
$version = $versionMatch.Groups[1].Value
$package = (Resolve-Path -LiteralPath $PackageRoot).Path
$compiler = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw "Inno Setup 6 compiler not found: $compiler" }
Write-Output "Inno Setup compiler: $((Get-Item -LiteralPath $compiler).VersionInfo.ProductVersion)"
foreach ($required in @('Sila.vst3\Contents\x86_64-win\Sila.vst3', 'LICENSE', 'README.md', 'PROCITAJ-ME.md',
                        'Sila-source.zip', 'JUCE-source.tar.gz', 'ThirdParty\INNO-LICENSE.txt')) {
    if (-not (Test-Path -LiteralPath (Join-Path $package $required) -PathType Leaf)) { throw "Missing installer input: $required" }
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$output = (Resolve-Path -LiteralPath $OutputRoot).Path
& $compiler "/DAppVersion=$version" "/DPackageRoot=$package" "/O$output" (Join-Path $PSScriptRoot 'Sila.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed: $LASTEXITCODE" }
$setupName = "Sila-$version-VST3-Setup-x64.exe"
$setup = Join-Path $output $setupName
if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) { throw 'Installer was not produced' }
$digest = (Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLowerInvariant()
"$digest  $setupName" | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii
Copy-Item -LiteralPath (Join-Path $repository 'docs\INSTALLER-HR.md') -Destination (Join-Path $output 'PROCITAJ-ME.md')
"Sila $version`nSource commit: $(git -C $repository rev-parse HEAD)`n" | Set-Content -LiteralPath (Join-Path $output 'BUILD-INFO.txt') -Encoding utf8
Write-Output "Built $setupName; SHA256 $digest"
