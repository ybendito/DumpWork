::%1 - input directory to load dump files from
::%2 - output directory to save data
::%3 - optional directory with PDB file
@echo off
if "%~1"=="" goto :usage
call :clean_output %2
for %%f in (%1\*.dmp) do call :call_kd %%f %2
del %2\dbg.txt
dumpwork.exe crd %2
goto :eof

:: call to stop: call :stophere "your text"
:stophere
echo %1
pause
goto :eof

:usage
echo %0 inputdir outputdir [pdbdir]
goto :eof

:clean_output
del /q %1\* 2>nul
mkdir %1 2>nul
goto :eof

::%1 - dump file to load
::%2 - output directory
:call_kd
echo running kd for %1
"c:\Program Files (x86)\Windows Kits\10\Debuggers\x64\kd.exe" -z %1 -c "!runaway 7; q" > %2\dbg.txt
set outdir=%2
set outfile=
set stage=0
echo parsing output
call :parse_output %2 dbg.txt
goto :eof

:nextstage
set /a stage=%stage%+1
goto :eof

::%1 - output directory
::%2 - file in it to parse
:parse_output
for /f "tokens=*" %%a in (%1\%2) do call :parse "%%a"
goto :eof

:parse
call :parse%stage% %1
goto :eof

:setfname
shift
shift
shift
shift
set time=%3
set outfile=%outdir%\%1-%2-%time::=-%.txt
del %outfile% 2> nul
goto :eof

:: looking for Debug session time:
:parse0
echo %1 | findstr /c:"Debug session time" > nul
if errorlevel 1 goto :eof
call :nextstage
call :setfname %~1
goto :eof

:: looking for "!runaway"
:parse1
echo %1 | findstr /c:"runaway" > nul
if errorlevel 1 goto :eof
call :nextstage
echo writing runaway data to %outfile%
goto :eof

:: looking for "!runaway"
:parse2
echo %1 | findstr /c:"quit:" > nul
if errorlevel 1 goto :writefile
call :nextstage
goto :eof
:writefile
echo %~1 >> %outfile%
goto :eof

::quit fount
:parse3
goto :eof
