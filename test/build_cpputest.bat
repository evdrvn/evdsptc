@echo off
cd ext\cpputest
del /Q cpputest_build\*.*
cd cpputest_build
cmake -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=.. rem -DCOVERAGE=ON ..
mingw32-make 
mingw32-make install
cd ..\..\..

