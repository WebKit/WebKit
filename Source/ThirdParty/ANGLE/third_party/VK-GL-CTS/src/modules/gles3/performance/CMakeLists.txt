# dEQP-GLES3.performance

set(DEQP_GLES3_PERFORMANCE_SRCS
	es3pBlendTests.cpp
	es3pBlendTests.hpp
	es3pPerformanceTests.cpp
	es3pPerformanceTests.hpp
	es3pShaderControlStatementTests.cpp
	es3pShaderControlStatementTests.hpp
	es3pTextureCases.cpp
	es3pTextureCases.hpp
	es3pTextureCountTests.cpp
	es3pTextureCountTests.hpp
	es3pTextureFilteringTests.cpp
	es3pTextureFilteringTests.hpp
	es3pTextureFormatTests.cpp
	es3pTextureFormatTests.hpp
	es3pShaderCompilerTests.cpp
	es3pShaderCompilerTests.hpp
	es3pShaderOptimizationTests.cpp
	es3pShaderOptimizationTests.hpp
	es3pShaderCompilationCases.hpp
	es3pShaderCompilationCases.cpp
	es3pRedundantStateChangeTests.cpp
	es3pRedundantStateChangeTests.hpp
	es3pStateChangeTests.cpp
	es3pStateChangeTests.hpp
	es3pStateChangeCallTests.cpp
	es3pStateChangeCallTests.hpp
	es3pBufferDataUploadTests.cpp
	es3pBufferDataUploadTests.hpp
	es3pShaderOperatorTests.cpp
	es3pShaderOperatorTests.hpp
	es3pDepthTests.hpp
	es3pDepthTests.cpp
	)

PCH(DEQP_GLES3_PERFORMANCE_SRCS ../../pch.cpp)

add_library(deqp-gles3-performance STATIC ${DEQP_GLES3_PERFORMANCE_SRCS})
target_link_libraries(deqp-gles3-performance deqp-gl-shared glutil tcutil ${DEQP_GLES3_LIBRARIES})
