# dEQP-GLES2.stress

set(DEQP_GLES2_STRESS_SRCS
	es2sStressTests.hpp
	es2sStressTests.cpp
	es2sMemoryTests.hpp
	es2sMemoryTests.cpp
	es2sLongRunningTests.hpp
	es2sLongRunningTests.cpp
	es2sSpecialFloatTests.hpp
	es2sSpecialFloatTests.cpp
	es2sVertexArrayTests.hpp
	es2sVertexArrayTests.cpp
	es2sDrawTests.hpp
	es2sDrawTests.cpp
	)

PCH(DEQP_GLES2_STRESS_SRCS ../../pch.cpp)

add_library(deqp-gles2-stress STATIC ${DEQP_GLES2_STRESS_SRCS})
target_link_libraries(deqp-gles2-stress deqp-gl-shared glutil tcutil ${DEQP_GLES2_LIBRARIES})
