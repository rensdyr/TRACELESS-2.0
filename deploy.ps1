# KENZO EXTERNAL v6 - Build & Deploy Script
# Usage: .\deploy.ps1 -GithubToken "your_github_token"

param(
    [string]$GithubToken = ""
)

$ErrorActionPreference = "Stop"

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "  KENZO EXTERNAL v6 Deployment" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "[1] Building solution..." -ForegroundColor Yellow
Push-Location $scriptPath

# Try to setup VS environment
$vsPaths = @(
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional\Common7\Tools\VsDevCmd.bat"
)

$vsDevCmd = $vsPaths | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($vsDevCmd) {
    Write-Host "    Setting up Visual Studio environment..." -ForegroundColor Gray
    & cmd /c "call `"$vsDevCmd`" && set" | ForEach-Object {
        if ($_ -match '=') {
            $name, $value = $_.Split('=', 2)
            Set-Item -Force -Path "env:\$name" -Value "$value" 2>$null
        }
    }
}

# Now find MSBuild
$msbuild = Get-Command msbuild -ErrorAction SilentlyContinue

if (-not $msbuild) {
    $msbuildPaths = @(
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe"
    )
    $msbuild = $msbuildPaths | Where-Object { Test-Path $_ } | Select-Object -First 1

    if (-not $msbuild) {
        Write-Host "[!] MSBuild not found. Run from Developer Command Prompt instead." -ForegroundColor Red
        Pop-Location
        exit 1
    }
} else {
    $msbuild = $msbuild.Source
}

Write-Host "    Using MSBuild" -ForegroundColor Gray

& msbuild Traceless.sln /p:Configuration=Release /p:Platform=x64 /m

if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Build failed!" -ForegroundColor Red
    Pop-Location
    exit 1
}

Pop-Location
Write-Host "[+] Build successful" -ForegroundColor Green
Write-Host ""

$dllPath = Join-Path $scriptPath "Traceless\Release\Traceless.dll"
$txtPath = Join-Path $scriptPath "Traceless\Release\Traceless.txt"

if (-not (Test-Path $dllPath)) {
    Write-Host "[!] DLL not found at $dllPath" -ForegroundColor Red
    exit 1
}

Write-Host "[2] Preparing DLL..." -ForegroundColor Yellow
$dllSize = (Get-Item $dllPath).Length / 1024
Write-Host "    DLL size: $([Math]::Round($dllSize, 2)) KB" -ForegroundColor Gray
Write-Host "[+] DLL ready" -ForegroundColor Green
Write-Host ""

if (-not $GithubToken) {
    Write-Host "[!] GitHub token required to upload!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Usage: .\deploy.ps1 -GithubToken 'ghp_your_token_here'" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "To generate a token:" -ForegroundColor Cyan
    Write-Host "  1. Go to https://github.com/settings/tokens" -ForegroundColor Gray
    Write-Host "  2. Create 'Personal access token (classic)'" -ForegroundColor Gray
    Write-Host "  3. Select 'gist' scope only" -ForegroundColor Gray
    Write-Host "  4. Copy token and run this script again" -ForegroundColor Gray
    Write-Host ""
    exit 1
}

Write-Host "[3] Uploading to gist..." -ForegroundColor Yellow

$dllBytes = [IO.File]::ReadAllBytes($dllPath)
$dllBase64 = [Convert]::ToBase64String($dllBytes)

$loaderPs1 = @"
# KENZO EXTERNAL v6 Loader
`$b64 = "$dllBase64"
`$dll = [Convert]::FromBase64String(`$b64)
`$path = "`$env:TEMP\kenzo.dll"
[IO.File]::WriteAllBytes(`$path, `$dll)
Write-Host '[+] KENZO EXTERNAL v6 injecting...' -ForegroundColor Green
# Load DLL at `$path
"@

$gistBody = @{
    description = "KENZO EXTERNAL v6 - Latest Build"
    public = $true
    files = @{
        "kenzo.ps1" = @{ content = $loaderPs1 }
        "kenzo.dll" = @{ content = $dllBase64 }
    }
} | ConvertTo-Json -Depth 10

$headers = @{
    Authorization = "token $GithubToken"
    Accept = "application/vnd.github.v3+json"
}

try {
    $response = Invoke-RestMethod -Uri "https://api.github.com/gists" `
        -Method Post `
        -Headers $headers `
        -Body $gistBody `
        -ContentType "application/json"

    $gistUrl = $response.files."kenzo.ps1".raw_url
    $oneLiner = "iex(irm '$gistUrl')"

    Write-Host "[+] Uploaded successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Gist URL: $($response.html_url)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "One-liner to load:" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host ""
    Write-Host $oneLiner -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Copy and paste in PowerShell to load KENZO EXTERNAL v6" -ForegroundColor Green
    Write-Host ""

} catch {
    Write-Host "[!] Upload failed: $_" -ForegroundColor Red
    exit 1
}
