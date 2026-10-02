; MFR-Lightning - Copyright (C) 2026 Music For Robots
; SPDX-License-Identifier: AGPL-3.0-or-later
;
; Inno Setup script. Built by scripts/package.ps1, which passes:
;   /DAppVersion=1.0.0 /DBuildDir=<...\Release> /DOutDir=<dist>

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{9499A81F-A4A3-4471-8B87-79435A054C6D}
AppName=MFR-Lightning
AppVersion={#AppVersion}
AppVerName=MFR-Lightning {#AppVersion}
AppPublisher=Music For Robots
AppPublisherURL=https://musicforrobots.com
AppSupportURL=https://github.com/music-for-robots/MFR-Lightning
DefaultDirName={autopf}\Music For Robots\MFR-Lightning
DefaultGroupName=Music For Robots
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir={#OutDir}
OutputBaseFilename=MFR-Lightning-{#AppVersion}-win64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=MFR-Lightning
UninstallDisplayIcon={app}\MFR-Lightning.exe

[Types]
Name: "full"; Description: "VST3 plugin and standalone app"
Name: "vst3"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (Common Files\VST3)"; Types: full vst3 custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full

[Files]
Source: "{#BuildDir}\VST3\MFR-Lightning.vst3\*"; DestDir: "{commoncf64}\VST3\MFR-Lightning.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\MFR-Lightning.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\MFR-Lightning"; Filename: "{app}\MFR-Lightning.exe"; Components: standalone
Name: "{group}\Uninstall MFR-Lightning"; Filename: "{uninstallexe}"

[UninstallDelete]
; The bundle is ours alone, so remove the whole folder (Inno leaves empty bundle dirs otherwise).
Type: filesandordirs; Name: "{commoncf64}\VST3\MFR-Lightning.vst3"
