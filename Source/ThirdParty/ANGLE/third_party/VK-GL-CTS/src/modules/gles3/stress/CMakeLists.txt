# dEQP-GLES3.stress

set(DEQP_GLES3_STRESS_SRCS
	es3sStressTests.hpp
	es3sStressTests.cpp
	es3sMemoryTests.hpp
	es3sMemoryTests.cpp
	es3sOcclusionQueryTests.hpp
	es3sOcclusionQueryTests.cpp
	es3sSyncTests.hpp
	es3sSyncTests.cpp
	es3sLongRunningTests.hpp
	es3sLongRunningTests.cpp
	es3sSpecialFloatTests.hpp
	es3sSpecialFloatTests.cpp
	es3sDrawTests.hpp
	es3sDrawTests.cpp
	es3sVertexArrayTests.hpp
	es3sVertexArrayTests.cpp
	es3sLongShaderTests.hpp
	es3sLongShaderTests.cpp
	es3sLongRunningShaderTests.cpp
	es3sLongRunningShaderTests.hpp
	)

PCH(DEQP_GLES3_STRESS_SRCS ../../pch.cpp)

add_library(deqp-gles3-stress STATIC ${DEQP_GLES3_STRESS_SRCS})
target_link_libraries(deqp-gles3-stress deqp-gl-shared glutil tcutil ${DEQP_GLES3_LIBRARIES})
