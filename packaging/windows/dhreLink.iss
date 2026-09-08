#define ProductName "dhreLink"
#define ProductVersion "1.0.0"
#define ProductFileVersion "1.0.0.0"
#define PublisherName "dhreian"

; Keep this AppId stable so upgrades and uninstall always address the same product.
#define ProductAppId "{{1F5079EA-698E-4A76-9355-28A14E7B92D8}"
#define StageRoot AddBackslash(SourcePath) + "..\..\dist\installer-stage"
#define OutputRoot AddBackslash(SourcePath) + "..\..\dist\installer"
#define Vst3Bundle StageRoot + "\vst3\dhreLink Sender.vst3"
#define Vst3Binary Vst3Bundle + "\Contents\x86_64-win\dhreLink Sender.vst3"
#define QuintessentialLicense Vst3Bundle + "\Contents\Resources\Quintessential-OFL.txt"
#define MontserratLicense Vst3Bundle + "\Contents\Resources\Montserrat-OFL.txt"
#define ObsPlugin StageRoot + "\obs\dhreLink"
#define ObsBinary ObsPlugin + "\bin\64bit\dhreLink.dll"
#define ObsDarkIcon ObsPlugin + "\data\dhreLink-dark.svg"
#define ObsLightIcon ObsPlugin + "\data\dhreLink-light.svg"

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
#ifnexist ObsDarkIcon
  #error "Missing OBS dark icon: " + ObsDarkIcon
#endif
#ifnexist ObsLightIcon
  #error "Missing OBS light icon: " + ObsLightIcon
#endif

#define Vst3SHA256 GetSHA256OfFile(Vst3Binary)
#define ObsSHA256 GetSHA256OfFile(ObsBinary)
#define QuintessentialLicenseSHA256 GetSHA256OfFile(QuintessentialLicense)
#define MontserratLicenseSHA256 GetSHA256OfFile(MontserratLicense)
#define ObsDarkIconSHA256 GetSHA256OfFile(ObsDarkIcon)
#define ObsLightIconSHA256 GetSHA256OfFile(ObsLightIcon)

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

OutputDir={#OutputRoot}
OutputBaseFilename=dhreLink-{#ProductVersion}-x64-Setup
Compression=lzma2/max
SolidCompression=yes
DiskSpanning=no

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin

; dhreLink has no standalone application. This folder only stores its uninstaller.
DefaultDirName={commonappdata}\dhreLink
CreateAppDir=no
UninstallFilesDir={commonappdata}\dhreLink\Installer
DisableDirPage=yes
UsePreviousAppDir=no
DisableProgramGroupPage=yes
WizardStyle=modern

Uninstallable=yes
CreateUninstallRegKey=yes
UninstallDisplayName=dhreLink
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
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[CustomMessages]
english.ObsTooOld=dhreLink requires OBS Studio 32.2 or newer. Update OBS and run this installer again.
spanish.ObsTooOld=dhreLink requiere OBS Studio 32.2 o posterior. Actualiza OBS y vuelve a ejecutar este instalador.

[Files]
; Standard machine-wide VST3 bundle location.
Source: "{#Vst3Bundle}\*"; DestDir: "{commoncf64}\VST3\dhreLink Sender.vst3"; \
  Excludes: "*.pdb,*.lib,*.exp,*.ilk"; \
  Flags: ignoreversion recursesubdirs createallsubdirs restartreplace uninsrestartdelete

; OBS-recommended machine-wide plugin layout.
Source: "{#ObsPlugin}\*"; DestDir: "{commonappdata}\obs-studio\plugins\dhreLink"; \
  Excludes: "*.pdb,*.lib,*.exp,*.ilk"; \
  Flags: ignoreversion recursesubdirs createallsubdirs restartreplace uninsrestartdelete

[UninstallDelete]
; Only remove dhreLink-owned folders; never shared VST3/OBS directories or user scenes.
Type: filesandordirs; Name: "{commoncf64}\VST3\dhreLink Sender.vst3"
Type: filesandordirs; Name: "{commonappdata}\obs-studio\plugins\dhreLink"

[Code]
var
  PendingRenameChecksumBefore: String;

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
    ExpandConstant('{commoncf64}\VST3\dhreLink Sender.vst3' +
      '\Contents\x86_64-win\dhreLink Sender.vst3'),
    '{#Vst3SHA256}', ReplacementPending, Failures);

  VerifyInstalledFile(
    'OBS source',
    ExpandConstant('{commonappdata}\obs-studio\plugins\dhreLink' +
      '\bin\64bit\dhreLink.dll'),
    '{#ObsSHA256}', ReplacementPending, Failures);

  VerifyInstalledFile(
    'Quintessential license',
    ExpandConstant('{commoncf64}\VST3\dhreLink Sender.vst3' +
      '\Contents\Resources\Quintessential-OFL.txt'),
    '{#QuintessentialLicenseSHA256}', ReplacementPending, Failures);

  VerifyInstalledFile(
    'Montserrat license',
    ExpandConstant('{commoncf64}\VST3\dhreLink Sender.vst3' +
      '\Contents\Resources\Montserrat-OFL.txt'),
    '{#MontserratLicenseSHA256}', ReplacementPending, Failures);

  VerifyInstalledFile(
    'OBS dark icon',
    ExpandConstant('{commonappdata}\obs-studio\plugins\dhreLink' +
      '\data\dhreLink-dark.svg'),
    '{#ObsDarkIconSHA256}', ReplacementPending, Failures);

  VerifyInstalledFile(
    'OBS light icon',
    ExpandConstant('{commonappdata}\obs-studio\plugins\dhreLink' +
      '\data\dhreLink-light.svg'),
    '{#ObsLightIconSHA256}', ReplacementPending, Failures);

  if Failures <> '' then
    RaiseException('dhreLink installation verification failed:' + Failures);
end;
