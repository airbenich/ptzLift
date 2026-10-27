@echo off
setlocal
cd /d "%~dp0"
where go >nul 2>&1
if errorlevel 1 (
  echo Install Go from https://go.dev/dl/ before building from source.
  pause
  exit /b 1
)
set GOOS=windows
set GOARCH=amd64
set CGO_ENABLED=0
go build -trimpath -ldflags "-s -w" -o FootPedalREST.exe .
if errorlevel 1 exit /b 1
go build -trimpath -ldflags "-s -w" -o RESTTestReceiver.exe ./cmd/testreceiver
if errorlevel 1 exit /b 1
echo Built FootPedalREST.exe and RESTTestReceiver.exe.
pause
