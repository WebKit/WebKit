# dEQP-GLES3

include_directories(
	../glshared
	.				# For child modules
	)

add_subdirectory(accuracy)
add_subdirectory(functional)
add_subdirectory(performance)
add_subdirectory(stress)

include_directories(
	accuracy
	functional
	performance
	stress
	)

set(DEQP_GLES3_SRCS
	tes3Context.cpp
	tes3Context.hpp
	tes3InfoTests.cpp
	tes3InfoTests.hpp
	tes3TestCase.cpp
	tes3TestCase.hpp
	tes3TestPackage.cpp
	tes3TestPackage.hpp
	tes3TestCaseWrapper.hpp
	tgl45es3TestPackage.cpp
	tgl45es3TestPackage.hpp
	)

set(DEQP_GLES3_LIBS
	deqp-gles3-accuracy
	deqp-gles3-functional
	deqp-gles3-performance
	deqp-gles3-stress
	tcutil
	glutil
	${DEQP_GLES3_LIBRARIES}
	)

add_deqp_module(deqp-gles3 "${DEQP_GLES3_SRCS}" "${DEQP_GLES3_LIBS}" "tcutil-platform" tes3TestPackageEntry.cpp)

# Data directories
add_data_dir(deqp-gles3 ../../data/gles3/data								gles3/data)
add_data_dir(deqp-gles3 ../../data/gles3/shaders							gles3/shaders)
add_data_dir(deqp-gles3 ../../external/graphicsfuzz/data/gles3/graphicsfuzz	gles3/graphicsfuzz)
