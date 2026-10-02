$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'Visual Studio vswhere.exe was not found.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ build tools were not found.' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "Missing $vcvars" }

& (Join-Path $PSScriptRoot '_build.bat') $vcvars
if ($LASTEXITCODE -ne 0) { throw "Application build failed: $LASTEXITCODE" }

$dist = Join-Path $PSScriptRoot 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$resource = Join-Path $dist 'setup.rc'
$manifest = Join-Path $dist 'setup.manifest'
Set-Content -LiteralPath $manifest -Encoding UTF8 -Value @'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3"><security><requestedPrivileges>
    <requestedExecutionLevel level="requireAdministrator" uiAccess="false"/>
  </requestedPrivileges></security></trustInfo>
</assembly>
'@
$escapedExe = (Join-Path $PSScriptRoot 'f1copy.exe').Replace('\', '\\')
$escapedManifest = $manifest.Replace('\', '\\')
Set-Content -LiteralPath $resource -Value "101 RCDATA `"$escapedExe`"`n1 24 `"$escapedManifest`"" -Encoding ASCII
$build = "call `"$vcvars`" >nul && rc.exe /fo `"$dist\setup.res`" `"$resource`" && cl.exe /nologo /utf-8 /O2 /EHsc /W3 /D UNICODE /D _UNICODE /Fe:`"$dist\f1copy-Setup.exe`" setup.cpp `"$dist\setup.res`" user32.lib shell32.lib advapi32.lib /link /MANIFEST:NO"
cmd.exe /d /s /c $build
if ($LASTEXITCODE -ne 0) { throw "Installer build failed: $LASTEXITCODE" }
Remove-Item -LiteralPath $resource, $manifest, (Join-Path $dist 'setup.res') -Force
Write-Host "Created $dist\f1copy-Setup.exe"
