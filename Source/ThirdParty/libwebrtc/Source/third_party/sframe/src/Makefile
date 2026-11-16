# This is just a convenience Makefile to avoid having to remember
# all the CMake commands and their arguments.

# Set CMAKE_GENERATOR in the environment to select how you build, e.g.:
#   CMAKE_GENERATOR=Ninja

BUILD_DIR=build
CLANG_FORMAT=clang-format -i

LIB=./build/libsframe.a
TEST_VECTOR_DIR=./build/test
TEST_BIN=./build/test/sframe_test

OPENSSL_1_1_MANIFEST=alternatives/OPENSSL_1_1
OPENSSL_3_MANIFEST=alternatives/OPENSSL_3
BORINGSSL_MANIFEST=alternatives/BORINGSSL

.PHONY: all dev dev1 devB tidy test clean cclean format

${LIB}: ${BUILD_DIR} src/* include/sframe/*
	cmake --build ${BUILD_DIR} --target sframe

${BUILD_DIR}: CMakeLists.txt test/CMakeLists.txt
	cmake -B${BUILD_DIR} .

dev: CMakeLists.txt test/CMakeLists.txt
	cmake -B${BUILD_DIR} -DCMAKE_BUILD_TYPE=Debug -DTESTING=ON \
		-DCLANG_TIDY=ON -DSANITIZERS=ON \
		-DCRYPTO=OPENSSL_3 -DVCPKG_MANIFEST_DIR=${OPENSSL_3_MANIFEST}

dev1: CMakeLists.txt test/CMakeLists.txt
	cmake -B${BUILD_DIR} -DCMAKE_BUILD_TYPE=Debug -DTESTING=ON \
		-DCLANG_TIDY=ON -DSANITIZERS=ON \
		-DCRYPTO=OPENSSL_1_1 -DVCPKG_MANIFEST_DIR=${OPENSSL_1_1_MANIFEST}

devB: CMakeLists.txt test/CMakeLists.txt
	cmake -B${BUILD_DIR} -DCMAKE_BUILD_TYPE=Debug -DTESTING=ON \
		-DCLANG_TIDY=ON -DSANITIZERS=ON \
		-DCRYPTO=BORINGSSL -DVCPKG_MANIFEST_DIR=${BORINGSSL_MANIFEST}

dev-nostd: CMakeLists.txt test/CMakeLists.txt
	cmake -B${BUILD_DIR} -DCMAKE_BUILD_TYPE=Debug -DCLANG_TIDY=ON -DTESTING=ON -DSANITIZERS=ON -DNO_ALLOC=ON .

${TEST_BIN}: ${LIB} test/*
	cmake --build ${BUILD_DIR} --target sframe_test

test: ${TEST_BIN}
	cd ${TEST_VECTOR_DIR} && ctest

dtest: ${TEST_BIN}
	cd test && ../${TEST_BIN}

dbtest: ${TEST_BIN}
	cd test && lldb ../${TEST_BIN}

clean:
	cmake --build ${BUILD_DIR} --target clean

cclean:
	rm -rf ${BUILD_DIR}

format:
	find include -iname "*.h" -or -iname "*.cpp" | xargs ${CLANG_FORMAT}
	find src -iname "*.h" -or -iname "*.cpp" | xargs ${CLANG_FORMAT}
	find test -iname "*.h" -or -iname "*.cpp" | xargs ${CLANG_FORMAT}

