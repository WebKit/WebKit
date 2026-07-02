# Simple GLESx reference

set(SGLR_SRCS
	sglrContext.cpp
	sglrContext.hpp
	sglrContextUtil.hpp
	sglrContextUtil.cpp
	sglrContextWrapper.cpp
	sglrContextWrapper.hpp
	sglrReferenceContext.cpp
	sglrReferenceContext.hpp
	sglrReferenceUtils.cpp
	sglrReferenceUtils.hpp
	sglrShaderProgram.cpp
	sglrShaderProgram.hpp
	sglrGLContext.cpp
	sglrGLContext.hpp
	)

add_library(glutil-sglr STATIC ${SGLR_SRCS})
target_link_libraries(glutil-sglr glutil tcutil referencerenderer ${DEQP_GLES2_LIBRARIES})
