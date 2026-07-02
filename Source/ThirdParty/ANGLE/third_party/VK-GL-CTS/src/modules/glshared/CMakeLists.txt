# Shared GL(ES) test code

include_directories(
	.
	)

set(DEQP_GL_SHARED_SRCS
	glsBuiltinPrecisionTests.cpp
	glsBuiltinPrecisionTests.hpp
	glsCalibration.cpp
	glsCalibration.hpp
	glsFboCompletenessTests.cpp
	glsFboCompletenessTests.hpp
	glsFboUtil.cpp
	glsFboUtil.hpp
	glsFragmentOpUtil.cpp
	glsFragmentOpUtil.hpp
	glsLifetimeTests.cpp
	glsLifetimeTests.hpp
	glsLongStressCase.cpp
	glsLongStressCase.hpp
	glsLongStressTestUtil.cpp
	glsLongStressTestUtil.hpp
	glsMemoryStressCase.cpp
	glsMemoryStressCase.hpp
	glsRandomShaderCase.cpp
	glsRandomShaderCase.hpp
	glsShaderConstExprTests.cpp
	glsShaderConstExprTests.hpp
	glsShaderLibrary.cpp
	glsShaderLibrary.hpp
	glsShaderLibraryCase.cpp
	glsShaderLibraryCase.hpp
	glsShaderPerformanceCase.cpp
	glsShaderPerformanceCase.hpp
	glsShaderRenderCase.cpp
	glsShaderRenderCase.hpp
	glsStateQueryUtil.cpp
	glsStateQueryUtil.hpp
	glsScissorTests.cpp
	glsScissorTests.hpp
	glsTextureStateQueryTests.cpp
	glsTextureStateQueryTests.hpp
	glsTextureTestUtil.cpp
	glsTextureTestUtil.hpp
	glsUniformBlockCase.cpp
	glsUniformBlockCase.hpp
	glsVertexArrayTests.cpp
	glsVertexArrayTests.hpp
	glsDrawTest.cpp
	glsDrawTest.hpp
	glsShaderExecUtil.cpp
	glsShaderExecUtil.hpp
	glsSamplerObjectTest.hpp
	glsSamplerObjectTest.cpp
	glsShaderPerformanceMeasurer.cpp
	glsShaderPerformanceMeasurer.hpp
	glsInteractionTestUtil.cpp
	glsInteractionTestUtil.hpp
	glsRandomShaderProgram.cpp
	glsRandomShaderProgram.hpp
	glsFragOpInteractionCase.cpp
	glsFragOpInteractionCase.hpp
	glsStateChangePerfTestCases.cpp
	glsStateChangePerfTestCases.hpp
	glsBufferTestUtil.cpp
	glsBufferTestUtil.hpp
	glsAttributeLocationTests.hpp
	glsAttributeLocationTests.cpp
	glsRandomUniformBlockCase.cpp
	glsRandomUniformBlockCase.hpp
	glsTextureBufferCase.hpp
	glsTextureBufferCase.cpp
	)

PCH(DEQP_GL_SHARED_SRCS ../pch.cpp)

add_library(deqp-gl-shared STATIC ${DEQP_GL_SHARED_SRCS})
target_link_libraries(deqp-gl-shared glutil glwrapper randomshaders tcutil glutil-sglr)
