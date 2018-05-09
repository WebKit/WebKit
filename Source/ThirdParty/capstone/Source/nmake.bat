:: Capstone disassembler engine (www.capstone-engine.org)
:: Build Capstone libs (capstone.dll & capstone.lib) on Windows with CMake & Nmake
:: By Nguyen Anh Quynh, 2017

cmake -DCMAKE_BUILD_TYPE=Release -G "NMake Makefiles" ..
nmake

