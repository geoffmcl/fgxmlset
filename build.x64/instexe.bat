@setlocal
@set TMPFIL=fgxmlset.exe
@set TMPEXE=Release\%TMPFIL%
@set TMPTARG=%TMPFIL%
@if NOT EXIST %TMPEXE% goto NOEXE
@set TMPINS=C:\MDOS
@if NOT EXIST %TMPINS%\nul goto NOINST
@set TMPDST=%TMPINS%\%TMPTARG%
@if NOT EXIST %TMPDST% goto DOCOPY
@fc4 -b -v0 -q %TMPEXE% %TMPDST%
@if ERRORLEVEL 1 goto DOCOPY
@echo.
@echo No change... Files the SAME... Nothing done...
@echo.
@goto END


:DOCOPY
@echo.
@call dirmin %TMPEXE%
@call dirmin %TMPDST%
@echo Files different. Will copy %TMPEXE% to %TMPDST%
@echo.
@echo *** CONTINUE? *** Only Ctrl+c aborts
@pause
@echo.
@echo Files different. Copying %TMPEXE% to %TMPDST%
@copy %TMPEXE% %TMPDST%
@echo.
@goto END

:NOEXE
@echo Can NOT locate %TMPEXE%! *** FIX ME ***
@goto END

:NOINST
@echo Can NOT locate install directory %TMPINS%! *** FIX ME ***
@goto END

:END

@REM EOF
