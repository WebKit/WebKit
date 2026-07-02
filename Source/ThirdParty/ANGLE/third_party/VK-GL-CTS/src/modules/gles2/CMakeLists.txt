# dEQP-GLES2

include_directories(
	accuracy
	functional
	performance
	randomshaders
	stress
	../glshared
	${CMAKE_CURRENT_SOURCE_DIR}
	)

add_subdirectory(accuracy)
add_subdirectory(functional)
add_subdirectory(performance)
add_subdirectory(stress)

set(DEQP_GLES2_SRCS
	tes2CapabilityTests.cpp
	tes2CapabilityTests.hpp
	tes2Context.cpp
	tes2Context.hpp
	tes2InfoTests.cpp
	tes2InfoTests.hpp
	tes2TestCase.cpp
	tes2TestCase.hpp
	tes2TestPackage.cpp
	tes2TestPackage.hpp
	)

set(DEQP_GLES2_LIBS
	deqp-gles2-accuracy
	deqp-gles2-functional
	deqp-gles2-performance
	deqp-gles2-stress
	tcutil
	glutil
	${DEQP_GLES2_LIBRARIES}
	)

add_deqp_module(deqp-gles2 "${DEQP_GLES2_SRCS}" "${DEQP_GLES2_LIBS}" "tcutil-platform" tes2TestPackageEntry.cpp)

# Data directories
add_data_dir(deqp-gles2 ../../data/gles2/data		gles2/data)
add_data_dir(deqp-gles2 ../../data/gles2/shaders	gles2/shaders)
