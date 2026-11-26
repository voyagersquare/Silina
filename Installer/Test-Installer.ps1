# Runs only on the disposable Windows CI runner. It never removes an existing installation.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SetupFile,
    [Parameter(Mandatory)][string]$PackageRoot,
    [Parameter(Mandatory)][string]$ValidatorPath,
    [string]$LogRoot = (Join-Path $PSScriptRoot '..\validation\installer')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows -or -not [Environment]::Is64BitProcess -or $env:GITHUB_ACTIONS -ne 'true') {
    throw 'This installation test is restricted to a disposable 64-bit Windows GitHub Actions runner.'
}
$setup = (Resolve-Path -LiteralPath $SetupFile).Path
$package = (Resolve-Path -LiteralPath $PackageRoot).Path
$validator = (Resolve-Path -LiteralPath $ValidatorPath).Path
New-Item -ItemType Directory -Force -Path $LogRoot | Out-Null
$logs = (Resolve-Path -LiteralPath $LogRoot).Path
$appRoot = Join-Path $env:RUNNER_TEMP ('Sila-Installer-' + [guid]::NewGuid().ToString('N'))
$vstRoot = Join-Path $env:CommonProgramFiles 'VST3\Sila.vst3'
$registry = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{A9921A06-5DD3-42B2-BBE9-1FB2C2147774}_is1'
foreach ($path in @($appRoot, $vstRoot, $registry)) {
    if (Test-Path -LiteralPath $path) { throw "Refusing to overwrite an existing installation: $path" }
}
function Assert-True([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-SameFile([string]$Expected, [string]$Actual) {
    Assert-True (Test-Path -LiteralPath $Actual -PathType Leaf) "Installed file missing: $Actual"
    Assert-True ((Get-FileHash -LiteralPath $Expected -Algorithm SHA256).Hash -eq (Get-FileHash -LiteralPath $Actual -Algorithm SHA256).Hash) "Installed file differs: $Actual"
}
function Assert-Payload {
    Assert-True (Test-Path -LiteralPath $registry) 'Windows uninstall registration missing'
    $sourceVst = Join-Path $package 'Sila.vst3'
    $sourceFiles = @(Get-ChildItem -LiteralPath $sourceVst -Recurse -File)
    $actualFiles = @(Get-ChildItem -LiteralPath $vstRoot -Recurse -File)
    Assert-True ($sourceFiles.Count -eq $actualFiles.Count) 'Installed VST3 bundle has the wrong file count'
    foreach ($file in $sourceFiles) {
        Assert-SameFile $file.FullName (Join-Path $vstRoot ([IO.Path]::GetRelativePath($sourceVst, $file.FullName)))
    }
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $appRoot 'Sila.exe'))) 'A Standalone application must not be installed'
    foreach ($file in @('LICENSE', 'README.md', 'PROCITAJ-ME.md')) { Assert-SameFile (Join-Path $package $file) (Join-Path $appRoot $file) }
    foreach ($file in @('Sila-source.zip', 'JUCE-source.tar.gz')) { Assert-SameFile (Join-Path $package $file) (Join-Path $appRoot "Source\$file") }
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $package 'ThirdParty') -File) {
        Assert-SameFile $file.FullName (Join-Path $appRoot "ThirdParty\$($file.Name)")
    }
    Assert-True (@(Get-ChildItem -LiteralPath $appRoot -Filter 'unins*.exe' -File).Count -eq 1) 'There must be one uninstaller'
}
function Install-Sila([string]$Name) {
    $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', "/DIR=`"$appRoot`"", "/LOG=`"$logs\$Name.log`"")
    $process = Start-Process -FilePath $setup -ArgumentList $arguments -Wait -PassThru
    Assert-True ($process.ExitCode -eq 0) "Installer failed ($Name): $($process.ExitCode)"
    Write-Output "Installed: $Name"
}

Install-Sila '01-install'
Assert-Payload
$versionMatch = [regex]::Match((Get-Content (Join-Path $PSScriptRoot '..\CMakeLists.txt') -Raw), 'project\(Sila VERSION ([0-9]+\.[0-9]+\.[0-9]+)')
$entry = Get-ItemProperty -LiteralPath $registry
Assert-True ($entry.DisplayVersion -eq $versionMatch.Groups[1].Value -and $entry.Publisher -eq 'Voyagersquare') 'Wrong installed version or publisher'

# Reinstall repairs a damaged plugin binary.
$binary = Join-Path $vstRoot 'Contents\x86_64-win\Sila.vst3'
$stream = [IO.File]::Open($binary, [IO.FileMode]::Append)
try { $stream.WriteByte(0) } finally { $stream.Dispose() }
Install-Sila '02-reinstall'
Assert-Payload

# Validate the plugin where hosts will find it.
$validationArgs = @('--strictness-level', '5', '--validate', "`"$vstRoot`"", '--output-dir', "`"$logs`"", '--timeout-ms', '120000')
$result = Start-Process -FilePath $validator -ArgumentList $validationArgs -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$logs\pluginval-stdout.txt" -RedirectStandardError "$logs\pluginval-stderr.txt"
Get-Content -LiteralPath "$logs\pluginval-stdout.txt"
Assert-True ($result.ExitCode -eq 0) 'Installed VST3 failed pluginval strictness 5'

$uninstaller = @(Get-ChildItem -LiteralPath $appRoot -Filter 'unins*.exe' -File)
$process = Start-Process -FilePath ($uninstaller[0].FullName) -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$logs\03-uninstall.log`"") -Wait -PassThru
Assert-True ($process.ExitCode -eq 0) "Uninstaller failed: $($process.ExitCode)"
Assert-True (-not (Test-Path -LiteralPath $registry)) 'Uninstall entry was not removed'
Assert-True (-not (Test-Path -LiteralPath $vstRoot)) 'Installed VST3 bundle was not removed'
Write-Output 'INSTALLER TESTS: PASS (install, payload hashes, reinstall repair, installed pluginval, uninstall)'
