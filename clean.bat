::
:: clean.bat Windows cleanup script for MulticopterSim
::
:: Copyright (C) 2020 Simon D. Levy
::
:: MIT License

::rm -rf WindowsNoEditor *.txt Makefile *.sln *.pri *.kdev4 *.pro *.*workspace .vs/ Build/ Binaries/ DerivedDataCache/ Intermediate/ Saved/

del /f /q *.sln *.vs > NUL

rmdir /q /s .vs Binaries Build DerivedDataCache Intermediate Saved > NUL:q
