@echo off
cd ext\cpputest
del /Q cpputest_build\*.*
cd cpputest_build
cmake -G "MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=.. -DCOVERAGE=OFF -DMEMORY_LEAK_DETECTION=ON ..
mingw32-make 
mingw32-make install
cd ..\..\..

