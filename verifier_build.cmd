@echo off
REM #####################################################################
REM This script build the MA on windows platform
REM #####################################################################
setlocal enabledelayedexpansion

set me=%~n0
set pwd=%~dp0

set VsDevCmd="C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools\VsDevCmd.bat"

set imvm_home=%pwd%\imvm

IF "%1"=="" (
  call:print_help
) ELSE IF "%2"=="" (
  call:print_help
) ELSE (
  call:imvm_build %2 %1
)
GOTO:EOF

:imvm_build
  echo. Building MA.... %1 %2
  set working_dir=%imvm_home%\msvc
  cd %working_dir%
  cd
  call %VsDevCmd%
  IF NOT %ERRORLEVEL% EQU 0 (
    echo. %me%: Visual Studio Dev Env could not be set
	EXIT /b %ERRORLEVEL%
  )
  call:imvm_build_util %1 %2
GOTO:EOF

:imvm_build_util
  setlocal
  echo. inside imvm_build_util %1 %2
  cd
  IF "%2"=="x86" (
    echo. calling with Win32 option
    msbuild imvm.sln /property:Configuration=%1;Platform=Win32
	IF NOT %ERRORLEVEL% EQU 0 (
	  echo. %me%: Build Failed
	  EXIT /b %ERRORLEVEL%
	)
  ) ELSE (
    echo. calling with x64 option
    msbuild imvm.sln /property:Configuration=%1;Platform=%2
    IF NOT %ERRORLEVEL% EQU 0 (
	  echo. %me%: Build Failed
	  EXIT /b %ERRORLEVEL%
	)
  )
  endlocal
GOTO:EOF

:print_help
  echo. "Usage: $0 Platform Configuration"
GOTO:EOF

endlocal