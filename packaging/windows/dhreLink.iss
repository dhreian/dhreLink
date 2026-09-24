#define ProductName "dhreLink"
#define ProductVersion "1.0"
#define ProductFileVersion "1.0.0.0"
#define PublisherName "dhreian"

; Keep this AppId stable so upgrades and uninstall always address the same product.
#define ProductAppId "{{1F5079EA-698E-4A76-9355-28A14E7B92D8}"
#define StageRoot AddBackslash(SourcePath) + "..\..\dist\installer-stage"
#define OutputRoot AddBackslash(SourcePath) + "..\..\dist\installer"
#define Vst3Bundle StageRoot + "\vst3\dhreLink Sender.vst3"
#define Vst3Binary Vst3Bundle + "\Contents\x86_64-win\dhreLink Sender.vst3"
#define Vst3InstallDir "{app}\dhreian\dhreLink Sender.vst3"
#define DefaultVst3InstallDir "{commoncf64}\VST3\dhreian\dhreLink Sender.vst3"
#define LegacyVst3InstallDir "{commoncf64}\VST3\dhreLink Sender.vst3"
#define QuintessentialLicense Vst3Bundle + "\Contents\Resources\Quintessential-OFL.txt"
#define MontserratLicense Vst3Bundle + "\Contents\Resources\Montserrat-OFL.txt"
#define InstallerIcon AddBackslash(SourcePath) + "..\..\assets\brand\dhreian-mark-purple.ico"
#define InstalledIconDir "{commonappdata}\dhreLink\Installer"
#define InstalledIcon InstalledIconDir + "\dhreian-mark-purple.ico"
#define ObsPlugin StageRoot + "\obs\dhreLink"
#define ObsBinary ObsPlugin + "\bin\64bit\dhreLink.dll"
#define ObsIcon ObsPlugin + "\data\dhreian-mark-purple-512.png"

; Refuse to compile an incomplete installer.
#ifnexist Vst3Binary
  #error "Missing staged VST3 binary: " + Vst3Binary
#endif
#ifnexist ObsBinary
  #error "Missing staged OBS binary: " + ObsBinary
#endif
#ifnexist QuintessentialLicense
  #error "Missing Quintessential license: " + QuintessentialLicense
#endif
#ifnexist MontserratLicense
  #error "Missing Montserrat license: " + MontserratLicense
#endif
#ifnexist InstallerIcon
  #error "Missing installer icon: " + InstallerIcon
#endif
#ifnexist ObsIcon
  #error "Missing OBS icon: " + ObsIcon
#endif

#define Vst3SHA256 GetSHA256OfFile(Vst3Binary)
#define ObsSHA256 GetSHA256OfFile(ObsBinary)
#define QuintessentialLicenseSHA256 GetSHA256OfFile(QuintessentialLicense)
#define MontserratLicenseSHA256 GetSHA256OfFile(MontserratLicense)
#define ObsIconSHA256 GetSHA256OfFile(ObsIcon)
#define InstallerIconSHA256 GetSHA256OfFile(InstallerIcon)

[Setup]
AppId={#ProductAppId}
AppName={#ProductName}
AppVersion={#ProductVersion}
AppVerName={#ProductName} {#ProductVersion}
AppPublisher={#PublisherName}
AppCopyright=Copyright (C) 2026 {#PublisherName}
VersionInfoVersion={#ProductFileVersion}
VersionInfoProductVersion={#ProductFileVersion}
VersionInfoProductTextVersion={#ProductVersion}
VersionInfoProductName={#ProductName}
VersionInfoCompany={#PublisherName}
VersionInfoDescription=Installs the dhreLink VST3 sender and OBS audio source
SetupIconFile={#InstallerIcon}

OutputDir={#OutputRoot}
OutputBaseFilename=dhreLink-v{#ProductVersion}-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
DiskSpanning=no

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin

; The selected folder is the VST3 root. The OBS plugin remains in its fixed location.
DefaultDirName={commoncf64}\VST3
AppendDefaultDirName=no
CreateAppDir=yes
UninstallFilesDir={commonappdata}\dhreLink\Installer
DisableDirPage=no
DirExistsWarning=no
UsePreviousAppDir=no
DisableProgramGroupPage=yes
WizardStyle=modern
ShowLanguageDialog=yes

Uninstallable=yes
CreateUninstallRegKey=yes
UninstallDisplayName=dhreLink
UninstallDisplayIcon={#InstalledIcon},0
UninstallLogMode=append
UpdateUninstallLogAppName=yes
UninstallRestartComputer=no

; Detect OBS or a DAW that currently has either plugin loaded.
CloseApplications=yes
CloseApplicationsFilter=*.dll,*.vst3
RestartApplications=no
RestartIfNeededByRun=no
SetupMutex=Global\dhreLink-Installer-1F5079EA698E4A76935528A14E7B92D8

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"; LicenseFile: "license-es.txt"
Name: "english"; MessagesFile: "compiler:Default.isl"; LicenseFile: "license-en.txt"

[CustomMessages]
english.ObsTooOld=dhreLink requires OBS Studio 32.2 or newer. Update OBS and run this installer again.
spanish.ObsTooOld=dhreLink requiere OBS Studio 32.2 o posterior. Actualiza OBS y vuelve a ejecutar este instalador.
spanish.VST3DirNote=Seleccione la carpeta VST3 que analiza su DAW. Se recomienda mantener la ruta predeterminada.
english.VST3DirNote=Choose a VST3 folder scanned by your DAW. Keeping the default path is recommended.

[Files]
; Keep the same icon on disk for Windows Settings after the setup EXE is removed.
Source: "{#InstallerIcon}"; DestDir: "{#InstalledIconDir}"; DestName: "dhreian-mark-purple.ico"; Flags: ignoreversion
Source: "license-es.txt"; DestDir: "{#InstalledIconDir}"; Flags: ignoreversion
Source: "license-en.txt"; DestDir: "{#InstalledIconDir}"; Flags: ignoreversion

; Install inside the selected VST3 folder, grouped under the publisher.
Source: "{#Vst3Bundle}\*"; DestDir: "{#Vst3InstallDir}"; \
  Excludes: "*.pdb,*.lib,*.exp,*.ilk"; \
  Flags: ignoreversion recursesubdirs createallsubdirs restartreplace

; OBS-recommended machine-wide plugin layout.
Source: "{#ObsPlugin}\*"; DestDir: "{commonappdata}\obs-studio\plugins\dhreLink"; \
  Excludes: "*.pdb,*.lib,*.exp,*.ilk"; \
  Flags: ignoreversion recursesubdirs createallsubdirs restartreplace

[InstallDelete]
; Remove the old VST3 location during upgrades so the DAW does not find two copies.
Type: filesandordirs; Name: "{#LegacyVst3InstallDir}"
Type: filesandordirs; Name: "{#DefaultVst3InstallDir}"

[UninstallDelete]
; Only remove dhreLink-owned folders; never shared VST3/OBS directories or user scenes.
Type: filesandordirs; Name: "{#Vst3InstallDir}"
Type: filesandordirs; Name: "{#DefaultVst3InstallDir}"
Type: filesandordirs; Name: "{#LegacyVst3InstallDir}"
Type: filesandordirs; Name: "{commonappdata}\obs-studio\plugins\dhreLink"

[Code]
var
  PendingRenameChecksumBefore: String;

procedure InitializeWizard;
begin
  WizardForm.SelectDirLabel.Caption := CustomMessage('VST3DirNote');
end;

function UninstallNeedRestart: Boolean;
begin
  Result := False;
end;

function InitializeSetup: Boolean;
var
  ObsDll: String;
  Major, Minor, Revision, Build: Word;
begin
  Result := True;
  ObsDll := ExpandConstant('{commonpf64}\obs-studio\bin\64bit\obs.dll');
  if FileExists(ObsDll) and
    GetVersionComponents(ObsDll, Major, Minor, Revision, Build) and
    ((Major < 32) or ((Major = 32) and (Minor < 2))) then
  begin
    MsgBox(CustomMessage('ObsTooOld'), mbError, MB_OK);
    Result := False;
  end;
end;

procedure AppendFailure(
  const Description, FileName: String;
  var Failures: String);
begin
  Failures := Failures + #13#10 + '- ' + Description + ': ' + FileName;
end;

procedure VerifyInstalledFile(
  const Description, FileName, ExpectedSHA256: String;
  const ReplacementPending: Boolean;
  var Failures: String);
var
  ActualSHA256: String;
begin
  if not FileExists(FileName) then
  begin
    AppendFailure(Description + ' is missing', FileName, Failures);
    Exit;
  end;

  ActualSHA256 := GetSHA256OfFile(FileName);
  if not SameText(ActualSHA256, ExpectedSHA256) then
  begin
    if ReplacementPending then
      Log(Description + ' will finish updating after Windows restarts: ' + FileName)
    else
      AppendFailure(Description + ' failed integrity verification', FileName, Failures);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Failures: String;
  ReplacementPending: Boolean;
begin
  if CurStep = ssInstall then
    PendingRenameChecksumBefore := MakePendingFileRenameOperationsChecksum;

  if CurStep <> ssPostInstall then
    Exit;

  ReplacementPending :=
    PendingRenameChecksumBefore <> MakePendingFileRenameOperationsChecksum;
  Failures := '';

  VerifyInstalledFile(
    'VST3 sender',
    ExpandConstant('{#Vst3InstallDir}' +
      '\Contents\x86_64-win\dhreLink Sender.vst3'),
    '{#Vst3SHA256}', ReplacementPending, Failures);

  VerifyInstalledFile(
    'OBS source',
    ExpandConstant('{commonappdata}\obs-studio\plugins\dhreLink' +
      '\bin\64bit\dhreLink.dll'),
    '{#ObsSHA256}', ReplacementPending, Failures);

  VerifyInstalledFile(
    'Quintessential license',
    ExpandConstant('{#Vst3InstallDir}' +
      '\Contents\Resources\Quintessential-OFL.txt'),
    '{#QuintessentialLicenseSHA256}', ReplacementPending, Failures);

  VerifyInstalledFile(
    'Montserrat license',
    ExpandConstant('{#Vst3InstallDir}' +
      '\Contents\Resources\Montserrat-OFL.txt'),
    '{#MontserratLicenseSHA256}', ReplacementPending, Failures);

  VerifyInstalledFile(
    'OBS icon',
    ExpandConstant('{commonappdata}\obs-studio\plugins\dhreLink' +
      '\data\dhreian-mark-purple-512.png'),
    '{#ObsIconSHA256}', ReplacementPending, Failures);

  VerifyInstalledFile(
    'Windows application icon',
    ExpandConstant('{#InstalledIcon}'),
    '{#InstallerIconSHA256}', ReplacementPending, Failures);

  if Failures <> '' then
    RaiseException('dhreLink installation verification failed:' + Failures);
end;
