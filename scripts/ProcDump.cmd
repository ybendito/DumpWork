:: -ma - full dump
:: -c 40 - 40% process cpu consumption (of total system available)
:: -s 2  - the condition (CPU consumption) is valid for 2 seconds
:: -r    - use PSS (process snapshot) - TODO: check what is better
:: -w DumpWork.exe s:\dumps
:: -n 5 after dump wait again for the condition and take next dump, up to 5
if "%1"=="" goto usage
if "%2"=="" goto usage
goto %2
:c
ProcDump.exe -ma -c %1 -s 2 -n 5 -r -w DumpWork.exe s:\dumps
goto :eof
ProcDump.exe -ma -p "\Process(DumpWork)\% Processor Time" %1 -s 2 -n 5 -r -w DumpWork.exe s:\dumps
:p
::
:usage
echo %0 40 c (for process CPU consumption of 40% from total system)
echo %0 500 p (for process CPU consumption of 500% from single core)
goto :eof
