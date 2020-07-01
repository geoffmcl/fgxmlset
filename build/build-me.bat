@setlocal
@set TMPBGN=%TIME%
@set TMPDRV=D:
@set TMPRT=%TMPDRV%\UTILS
@set TMPVER=1
@set TMPPRJ=fgxmlset
@set TMPMV=
@set TMPSRC=%TMPRT%\%TMPPRJ%%TMPMV%
@set TMPBGN=%TIME%
@set TMPINS=D:\Projects\3rdParty.x64
@set TMPLOG=bldlog-1.txt
@set TMPSG=D:\FG\next\install\msvc140-64

@set TMPOPTS=-DCMAKE_INSTALL_PREFIX=%TMPINS%
@if EXIST %TMPSG%\nul (
@set TMPOPTS=%TMPOPTS% -DCMAKE_PREFIX_PATH:PATH=%TMPSG%
)

@REM Add this to DEBUG what is happening in the FindSimGear.cmake modules
@REM set TMPOPTS=%TMPOPTS% -DSG_DBG_FINDING:BOOL=TRUE

@call chkmsvc %TMPPRJ%

:RPT
@if "%~1x" == "x" goto GOTCMD
@set TMPOPTS=%TMPOPTS% %1
@shift
@goto RPT
:GOTCMD

@echo Build %TMPPRJ% %DATE% %TIME% in %CD% > %TMPLOG%
@echo Build source %TMPSRC%... output to %TMPLOG%
@echo Build source %TMPSRC%... output to %TMPLOG% >> %TMPLOG%

@REM Set any environment variables needed
@set BOOST_ROOT=C:\Projects\boost_1_72_0
@echo Set BOOST_ROOT=%BOOST_ROOT% >> %TMPLOG%
@if NOT EXIST %TMPSG%\nul goto DNSG
@set SIMGEAR_DIR=%TMPSG%
@echo Set SIMGEAR_DIR=%SIMGEAR_DIR% >> %TMPLOG%
:DGSG

@echo Doing: 'cmake %TMPSRC% %TMPOPTS%'
@echo Doing: 'cmake %TMPSRC% %TMPOPTS%' >> %TMPLOG%
@cmake %TMPSRC% %TMPOPTS% >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR1

@echo Doing 'cmake --build . --config Debug'
@echo Doing 'cmake --build . --config Debug' >> %TMPLOG%
@cmake --build . --config Debug >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR2

@echo Doing 'cmake --build . --config Release'
@echo Doing 'cmake --build . --config Release' >> %TMPLOG%
@cmake --build . --config Release >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR3

@REM fa4 "TODO" -c %TMPLOG%
@echo.
@echo See %TMPLOG%... Appears OK... 
@echo.

@call elapsed %TMPBGN%

@echo No cmake install at this time... 
@echo Maybe use instexe.bat after setting install location...
@echo.
@REM stop here
@goto END

@echo Continue with install? Only Ctrl+c aborts...
@echo.
@pause

cmake --build . --config Debug  --target INSTALL >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR4

cmake --build . --config Release  --target INSTALL >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR5


@call elapsed %TMPBGN%
@echo All done... see %TMPLOG%
@REM echo Define -DASSIMP_BUILD_STATIC_LIB=TRUE to use the static libraries, else the DLL... add STATIC to the cmd

@goto END

:ERR1
@echo cmake configuration or generations ERROR
@goto END1

:ERR2
@echo cmake build Debug ERROR
@fa4 "fatal" %TMPLOG%
@goto END1

:ERR3
@echo cmake build Release ERROR
@goto END1

:ERR4
@echo cmake install debug ERROR
@goto END1

:ERR5
@echo cmake install release ERROR
@goto END1

:END1
@echo See %TMPLOG% for details...
:END

@REM eof
