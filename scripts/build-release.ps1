[CmdletBinding()]
param(
  [string]$Vst3SdkRoot = (Join-Path $PSScriptRoot '..\.deps\vst3sdk'),
  [string]$ObsSourceDirectory = (Join-Path $PSScriptRoot '..\.deps\obs-studio'),
  [string]$ObsRuntimeDll = 'C:\Program Files\obs-studio\bin\64bit\obs.dll'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Checked {
  param(
    [Parameter(Mandatory)] [string]$FilePath,
    [Parameter(Mandatory)] [string[]]$Arguments
  )

  & $FilePath @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed with exit code ${LASTEXITCODE}: $FilePath"
  }
}

function Resolve-Program {
  param(
    [Parameter(Mandatory)] [string]$CommandName,
    [Parameter(Mandatory)] [string[]]$Fallbacks
  )

  $command = Get-Command $CommandName -ErrorAction SilentlyContinue
  if ($null -ne $command) {
    return $command.Source
  }

  foreach ($candidate in $Fallbacks) {
    if (Test-Path -LiteralPath $candidate) {
      return $candidate
    }
  }

  throw "Required tool was not found: $CommandName"
}

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$vst3Root = [IO.Path]::GetFullPath($Vst3SdkRoot)
$obsSourceRoot = [IO.Path]::GetFullPath($ObsSourceDirectory)
$obsRuntime = [IO.Path]::GetFullPath($ObsRuntimeDll)
$buildRoot = Join-Path $repositoryRoot 'build-release'
$stageRoot = Join-Path $repositoryRoot 'dist\installer-stage'
$installerRoot = Join-Path $repositoryRoot 'dist\installer'
$obsLibRoot = Join-Path $repositoryRoot '.deps\obs-sdk\lib'
$obsImportLibrary = Join-Path $obsLibRoot 'obs.lib'
$obsDefinition = Join-Path $repositoryRoot 'src\obs\obs.def'

$expectedBuildRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'build-release'))
$resolvedBuildRoot = [IO.Path]::GetFullPath($buildRoot)
if (-not [StringComparer]::OrdinalIgnoreCase.Equals($expectedBuildRoot, $resolvedBuildRoot)) {
  throw "Refusing to clean an unexpected build directory: $resolvedBuildRoot"
}
if ([IO.Directory]::Exists($resolvedBuildRoot)) {
  [IO.Directory]::Delete($resolvedBuildRoot, $true)
}

if (-not (Test-Path -LiteralPath (Join-Path $vst3Root 'CMakeLists.txt'))) {
  throw "VST3 SDK not found. Run scripts\bootstrap-dependencies.ps1 first: $vst3Root"
}
if (-not (Test-Path -LiteralPath (Join-Path $obsSourceRoot 'libobs\obs-module.h'))) {
  throw "OBS source tree not found. Run scripts\bootstrap-dependencies.ps1 first: $obsSourceRoot"
}
if (-not (Test-Path -LiteralPath $obsRuntime)) {
  throw "OBS runtime not found. Install OBS Studio 32.2.x or pass -ObsRuntimeDll: $obsRuntime"
}
if (-not (Test-Path -LiteralPath $obsDefinition)) {
  throw "OBS import definition is missing: $obsDefinition"
}

$obsVersion = (Get-Item -LiteralPath $obsRuntime).VersionInfo.ProductVersion
if ($obsVersion -notmatch '^32\.2(?:\.|$)') {
  throw "This release targets OBS Studio 32.2.x, but obs.dll reports version $obsVersion"
}

$cmake = Resolve-Program 'cmake.exe' @('C:\Program Files\CMake\bin\cmake.exe')
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctest)) {
  throw "CTest was not found beside CMake: $ctest"
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
  throw 'Visual Studio 2022 Build Tools with the Desktop development with C++ workload is required.'
}
$visualStudioRoot = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if ([string]::IsNullOrWhiteSpace($visualStudioRoot)) {
  throw 'The Visual Studio x64 C++ tools were not found.'
}
$libTool = Get-ChildItem -Path (Join-Path $visualStudioRoot 'VC\Tools\MSVC\*\bin\Hostx64\x64\lib.exe') |
  Sort-Object FullName -Descending |
  Select-Object -First 1
if ($null -eq $libTool) {
  throw 'The x64 Microsoft librarian was not found.'
}
$dumpbin = Join-Path $libTool.DirectoryName 'dumpbin.exe'
if (-not (Test-Path -LiteralPath $dumpbin)) {
  throw "dumpbin.exe was not found: $dumpbin"
}

$iscc = Resolve-Program 'ISCC.exe' @(
  "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
  "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
  "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
)

$exportReport = (& $dumpbin /nologo /exports $obsRuntime 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) {
  throw "Could not inspect OBS exports: $obsRuntime"
}
foreach ($requiredExport in @(
  'obs_find_module_file',
  'obs_properties_add_text',
  'obs_properties_create',
  'obs_property_text_set_info_type',
  'obs_register_source_s',
  'obs_source_output_audio',
  'obs_source_update_properties',
  'os_gettime_ns'
)) {
  if ($exportReport -notmatch "(?m)\b$([regex]::Escape($requiredExport))\b") {
    throw "The OBS runtime does not export ${requiredExport}: $obsRuntime"
  }
}

[IO.Directory]::CreateDirectory($obsLibRoot) | Out-Null
Invoke-Checked $libTool.FullName @(
  "/def:$obsDefinition", '/machine:x64', "/out:$obsImportLibrary"
)

Invoke-Checked $cmake @(
  '--fresh', '-S', $repositoryRoot, '-B', $buildRoot,
  '-G', 'Visual Studio 17 2022', '-A', 'x64',
  '-DDHRELINK_BUILD_TESTS=ON',
  '-DDHRELINK_BUILD_VST3=ON',
  '-DDHRELINK_BUILD_OBS=ON',
  "-DVST3_SDK_ROOT=$vst3Root",
  "-DOBS_SOURCE_DIR=$obsSourceRoot",
  "-DOBS_LIBOBS_LIBRARY=$obsImportLibrary",
  "-DOBS_RUNTIME_DLL=$obsRuntime"
)
Invoke-Checked $cmake @('--build', $buildRoot, '--config', 'Release', '--parallel')
Invoke-Checked $ctest @('--test-dir', $buildRoot, '-C', 'Release', '--output-on-failure')

$expectedStageRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'dist\installer-stage'))
$resolvedStageRoot = [IO.Path]::GetFullPath($stageRoot)
if (-not [StringComparer]::OrdinalIgnoreCase.Equals($expectedStageRoot, $resolvedStageRoot)) {
  throw "Refusing to clean an unexpected staging directory: $resolvedStageRoot"
}
if ([IO.Directory]::Exists($resolvedStageRoot)) {
  [IO.Directory]::Delete($resolvedStageRoot, $true)
}
[IO.Directory]::CreateDirectory($resolvedStageRoot) | Out-Null

Invoke-Checked $cmake @(
  '--install', $buildRoot, '--config', 'Release', '--prefix', $resolvedStageRoot
)

$stagedVst3 = Join-Path $resolvedStageRoot 'vst3\dhreLink Sender.vst3\Contents\x86_64-win\dhreLink Sender.vst3'
$stagedObs = Join-Path $resolvedStageRoot 'obs\dhreLink\bin\64bit\dhreLink.dll'
$stagedObsDarkIcon = Join-Path $resolvedStageRoot 'obs\dhreLink\data\dhreLink-dark.svg'
$stagedObsLightIcon = Join-Path $resolvedStageRoot 'obs\dhreLink\data\dhreLink-light.svg'
$stagedQuintessentialLicense = Join-Path $resolvedStageRoot 'vst3\dhreLink Sender.vst3\Contents\Resources\Quintessential-OFL.txt'
$stagedMontserratLicense = Join-Path $resolvedStageRoot 'vst3\dhreLink Sender.vst3\Contents\Resources\Montserrat-OFL.txt'
foreach ($requiredFile in @(
  $stagedVst3,
  $stagedObs,
  $stagedObsDarkIcon,
  $stagedObsLightIcon,
  $stagedQuintessentialLicense,
  $stagedMontserratLicense
)) {
  if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
    throw "Release staging is incomplete: $requiredFile"
  }
}

$expectedInstallerRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'dist\installer'))
$resolvedInstallerRoot = [IO.Path]::GetFullPath($installerRoot)
if (-not [StringComparer]::OrdinalIgnoreCase.Equals($expectedInstallerRoot, $resolvedInstallerRoot)) {
  throw "Refusing to clean an unexpected installer directory: $resolvedInstallerRoot"
}
if ([IO.Directory]::Exists($resolvedInstallerRoot)) {
  [IO.Directory]::Delete($resolvedInstallerRoot, $true)
}
[IO.Directory]::CreateDirectory($resolvedInstallerRoot) | Out-Null
Invoke-Checked $iscc @((Join-Path $repositoryRoot 'packaging\windows\dhreLink.iss'))

$installer = Join-Path $installerRoot 'dhreLink-1.0.0-x64-Setup.exe'
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
  throw "Installer was not produced: $installer"
}

$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
Write-Host "Release ready: $installer"
Write-Host "SHA-256: $hash"
