@setlocal
@set TMPEXE=Release\fgxmlset.exe
@if NOT EXIST %TMPEXE% goto NOEXE
@set TMPXML=X:\fgdata\Aircraft\c172p\c172p-set.xml
@if NOT EXIST %TMPXML% goto NOXML

%TMPEXE% -r X:\fgdata %TMPXML% %*

@goto END

:NOEXE
@echo Error: Can NOT locate EXE %TMPEXE%! *** FIX ME ***
@goto END

:NOXML
@echo Error: Can NOT locate XML %TMPXML%! *** FIX ME ***
@goto END

:END

