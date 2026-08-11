# dEQP-GLES2.accuracy

set(DEQP_GLES2_ACCURACY_SRCS
	es2aAccuracyTests.cpp
	es2aAccuracyTests.hpp
	es2aTextureFilteringTests.cpp
	es2aTextureFilteringTests.hpp
	es2aTextureMipmapTests.cpp
	es2aTextureMipmapTests.hpp
	es2aVaryingInterpolationTests.cpp
	es2aVaryingInterpolationTests.hpp
	)

PCH(DEQP_GLES2_ACCURACY_SRCS ../../pch.cpp)

add_library(deqp-gles2-accuracy STATIC ${DEQP_GLES2_ACCURACY_SRCS})
target_link_libraries(deqp-gles2-accuracy deqp-gl-shared glutil tcutil ${DEQP_GLES2_LIBRARIES})
