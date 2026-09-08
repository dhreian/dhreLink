[CmdletBinding()]
param(
  [string]$DependencyDirectory = (Join-Path $PSScriptRoot '..\.deps'),
  [string]$Vst3SdkTag = 'v3.8.1_build_84',
  [string]$ObsTag = '32.2.2'
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

$git = Get-Command git.exe -ErrorAction SilentlyContinue
if ($null -eq $git) {
  throw 'Git is required to download the official VST3 and OBS source trees.'
}

$dependencyRoot = [IO.Path]::GetFullPath($DependencyDirectory)
[IO.Directory]::CreateDirectory($dependencyRoot) | Out-Null

$vst3Root = Join-Path $dependencyRoot 'vst3sdk'
if (-not (Test-Path -LiteralPath (Join-Path $vst3Root 'CMakeLists.txt'))) {
  if (Test-Path -LiteralPath $vst3Root) {
    throw "The VST3 dependency directory is incomplete: $vst3Root"
  }

  Invoke-Checked $git.Source @(
    'clone', '--depth', '1', '--branch', $Vst3SdkTag,
    '--recurse-submodules', '--shallow-submodules',
    'https://github.com/steinbergmedia/vst3sdk.git', $vst3Root
  )
}

$obsRoot = Join-Path $dependencyRoot 'obs-studio'
if (-not (Test-Path -LiteralPath (Join-Path $obsRoot 'libobs\obs-module.h'))) {
  if (Test-Path -LiteralPath $obsRoot) {
    throw "The OBS dependency directory is incomplete: $obsRoot"
  }

  Invoke-Checked $git.Source @(
    'clone', '--depth', '1', '--branch', $ObsTag,
    'https://github.com/obsproject/obs-studio.git', $obsRoot
  )
}

Write-Host "Dependencies are ready in $dependencyRoot"
