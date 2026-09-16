# Regenerates the translation catalogs from the source tree.
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\update-translations.ps1 [-Preset release] [-Check]
#
# Builds the patchy_update_translations CMake target, which runs Qt's lupdate over every
# Patchy-owned source (CMakeLists.txt owns the source list and options) so that
# translations\patchy_en.ts (the English template) and every translations\patchy_<code>.ts
# carry exactly the strings the code uses, then prints the unfinished count per language.
# -Check skips lupdate and only reports; it fails when any language has unfinished
# entries. The UI test translation_catalogs_are_complete enforces the same rule, and
# translation_template_is_current fails when this script has not been run after a
# string change. See docs/localization.md.
param(
  [string]$Preset = "release",
  [switch]$Check
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repo "build\$Preset"
$cache = Join-Path $buildDir "CMakeCache.txt"
if (-not (Test-Path $cache)) {
  throw "No configured build at $buildDir. Configure the '$Preset' preset first."
}

if (-not $Check) {
  $cmakeLine = Get-Content $cache | Where-Object { $_ -like "CMAKE_COMMAND:INTERNAL=*" } | Select-Object -First 1
  if (-not $cmakeLine) { throw "CMAKE_COMMAND not found in $cache" }
  $cmake = $cmakeLine.Substring("CMAKE_COMMAND:INTERNAL=".Length)
  Push-Location $repo
  try {
    if ($IsWindows -or $env:OS -eq "Windows_NT") {
      # Same developer environment and below-normal priority as every other build
      # (AGENTS.md); lupdate itself is single-threaded and takes a few seconds.
      & cmd /s /c "scripts\vs-env.bat -arch=x64 -host_arch=x64 >nul && start """" /b /wait /belownormal ""$cmake"" --build --preset $Preset --target patchy_update_translations"
    } else {
      & nice -n 10 $cmake --build --preset $Preset --target patchy_update_translations
    }
    if ($LASTEXITCODE -ne 0) { throw "lupdate failed (exit $LASTEXITCODE)" }
  } finally {
    Pop-Location
  }
}

$catalogs = Get-ChildItem -Path (Join-Path $repo "translations") -Filter "patchy_*.ts" | Sort-Object Name
$failed = $false
Write-Host ("{0,-16} {1,8} {2,11}" -f "catalog", "strings", "unfinished")
foreach ($catalog in $catalogs) {
  [xml]$doc = Get-Content -LiteralPath $catalog.FullName -Encoding UTF8 -Raw
  $messages = $doc.SelectNodes("//message")
  $unfinished = $doc.SelectNodes("//message/translation[@type='unfinished']")
  $isTemplate = $catalog.Name -eq "patchy_en.ts"
  $note = ""
  if ($isTemplate) { $note = "  (template: unfinished by design)" }
  elseif ($unfinished.Count -gt 0) { $failed = $true; $note = "  <- needs translation" }
  Write-Host ("{0,-16} {1,8} {2,11}{3}" -f $catalog.Name, $messages.Count, $unfinished.Count, $note)
}
if ($failed) {
  Write-Host "Fill the unfinished entries (docs/localization.md) and rerun with -Check."
  exit 1
}
