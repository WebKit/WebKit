# dEQP-GLES2.performance

set(DEQP_GLES2_PERFORMANCE_SRCS
	es2pBlendTests.cpp
	es2pBlendTests.hpp
	es2pPerformanceTests.cpp
	es2pPerformanceTests.hpp
	es2pShaderControlStatementTests.cpp
	es2pShaderControlStatementTests.hpp
	es2pShaderOperatorTests.cpp
	es2pShaderOperatorTests.hpp
	es2pTextureCases.cpp
	es2pTextureCases.hpp
	es2pTextureCountTests.cpp
	es2pTextureCountTests.hpp
	es2pTextureFilteringTests.cpp
	es2pTextureFilteringTests.hpp
	es2pTextureFormatTests.cpp
	es2pTextureFormatTests.hpp
	es2pShaderCompilerTests.cpp
	es2pShaderCompilerTests.hpp
	es2pTextureUploadTests.cpp
	es2pTextureUploadTests.hpp
	es2pStateChangeCallTests.hpp
	es2pStateChangeCallTests.cpp
	es2pStateChangeTests.hpp
	es2pStateChangeTests.cpp
	es2pRedundantStateChangeTests.hpp
	es2pRedundantStateChangeTests.cpp
	es2pDrawCallBatchingTests.hpp
	es2pDrawCallBatchingTests.cpp
	es2pShaderOptimizationTests.hpp
	es2pShaderOptimizationTests.cpp
	es2pShaderCompilationCases.hpp
	es2pShaderCompilationCases.cpp
	)

PCH(DEQP_GLES2_PERFORMANCE_SRCS ../../pch.cpp)

add_library(deqp-gles2-performance STATIC ${DEQP_GLES2_PERFORMANCE_SRCS})
target_link_libraries(deqp-gles2-performance deqp-gl-shared glutil tcutil ${DEQP_GLES2_LIBRARIES})
