
set(RSG_SRCS
	rsgBinaryOps.cpp
	rsgBinaryOps.hpp
	rsgBuiltinFunctions.cpp
	rsgBuiltinFunctions.hpp
	rsgDefs.cpp
	rsgDefs.hpp
	rsgExecutionContext.cpp
	rsgExecutionContext.hpp
	rsgExpression.cpp
	rsgExpression.hpp
	rsgExpressionGenerator.cpp
	rsgExpressionGenerator.hpp
	rsgFunctionGenerator.cpp
	rsgFunctionGenerator.hpp
	rsgGeneratorState.cpp
	rsgGeneratorState.hpp
	rsgNameAllocator.cpp
	rsgNameAllocator.hpp
	rsgParameters.cpp
	rsgParameters.hpp
	rsgPrettyPrinter.cpp
	rsgPrettyPrinter.hpp
	rsgProgramGenerator.cpp
	rsgProgramGenerator.hpp
	rsgSamplers.cpp
	rsgSamplers.hpp
	rsgShader.cpp
	rsgShader.hpp
	rsgProgramExecutor.cpp
	rsgProgramExecutor.hpp
	rsgShaderGenerator.cpp
	rsgShaderGenerator.hpp
	rsgStatement.cpp
	rsgStatement.hpp
	rsgToken.cpp
	rsgToken.hpp
	rsgUtils.cpp
	rsgUtils.hpp
	rsgVariable.cpp
	rsgVariable.hpp
	rsgVariableManager.cpp
	rsgVariableManager.hpp
	rsgVariableType.cpp
	rsgVariableType.hpp
	rsgVariableValue.cpp
	rsgVariableValue.hpp
	)

PCH(RSG_SRCS ../pch.cpp)

add_library(randomshaders STATIC ${RSG_SRCS})
target_link_libraries(randomshaders tcutil)

if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX OR DE_OS_IS_OSX)
	add_executable(rsgtest rsgTest.cpp)
	target_link_libraries(rsgtest tcutil-platform randomshaders)
endif ()
