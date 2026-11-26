; Build with Installer/Build-Installer.ps1 after collecting the validated package.
; Installs only the VST3 plugin (no Standalone application).
#ifndef AppVersion
  #error "AppVersion must be supplied by the build script"
#endif
#ifndef PackageRoot
  #error "PackageRoot must be supplied by the build script"
#endif

[Setup]
; Keep this ID stable for future upgrades and the Windows uninstall entry.
AppId={{A9921A06-5DD3-42B2-BBE9-1FB2C2147774}
AppName=Sila
AppVersion={#AppVersion}
AppPublisher=Voyagersquare
AppPublisherURL=https://github.com/voyagersquare/Silina
AppSupportURL=https://github.com/voyagersquare/Silina/issues
DefaultDirName={autopf}\Voyagersquare\Sila
DefaultGroupName=Sila
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
WizardStyle=modern
LicenseFile={#PackageRoot}\LICENSE
OutputBaseFilename=Sila-{#AppVersion}-VST3-Setup-x64
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
UninstallDisplayName=Sila {#AppVersion} VST3 (64-bit)
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany=Voyagersquare
VersionInfoDescription=Sila tape saturation VST3 installer

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SelectDirLabel3=Licences, instructions and source code go into this folder.%nThe Sila VST3 plugin is always installed to C:\Program Files\Common Files\VST3.

[Files]
Source: "{#PackageRoot}\Sila.vst3\*"; DestDir: "{commoncf64}\VST3\Sila.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PackageRoot}\PROCITAJ-ME.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\ThirdParty\*"; DestDir: "{app}\ThirdParty"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PackageRoot}\Sila-source.zip"; DestDir: "{app}\Source"; Flags: ignoreversion
Source: "{#PackageRoot}\JUCE-source.tar.gz"; DestDir: "{app}\Source"; Flags: ignoreversion

[Icons]
Name: "{group}\Upute za Silu"; Filename: "{app}\PROCITAJ-ME.md"
Name: "{group}\Uninstall Sila"; Filename: "{uninstallexe}"
