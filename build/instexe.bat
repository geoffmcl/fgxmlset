@setlocal
@set TMPFIL=fgxmlset.exe
@set TMPEXE=Release\%TMPFIL%
@if NOT EXIST %TMPEXE% goto NOEXE
@set TMPINS=C:\MDOS
@if NOT EXIST %TMPINS%\nul goto NOINST
@set TMPDST=%TMPINS%\%TMPFIL%
@if NOT EXIST %TMPDST% goto DOCOPY
@fc4 -b -v0 -q %TMPEXE% %TMPDST%
@if ERRORLEVEL 1 goto DOCOPY
@echo No change... Nothing done...
@goto END


:DOCOPY
@echo Copying %TMPEXE% to %TMPDST%
@copy %TMPEXE% %TMPDST%
@goto END

:NOEXE
@echo Can NOT locate %TMPEXE%! *** FIX ME ***
@goto END

:NOINST
@echo Can NOT locate install directory %TMPINS%! *** FIX ME ***
@goto END

:END

@REM EOF
