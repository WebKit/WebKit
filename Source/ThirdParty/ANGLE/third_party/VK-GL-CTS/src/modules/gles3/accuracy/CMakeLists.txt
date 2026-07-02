# dEQP-GLES3.accuracy

set(DEQP_GLES3_ACCURACY_SRCS
	es3aAccuracyTests.cpp
	es3aAccuracyTests.hpp
	es3aTextureFilteringTests.cpp
	es3aTextureFilteringTests.hpp
	es3aTextureMipmapTests.cpp
	es3aTextureMipmapTests.hpp
	es3aVaryingInterpolationTests.cpp
	es3aVaryingInterpolationTests.hpp
	)

PCH(DEQP_GLES3_ACCURACY_SRCS ../../pch.cpp)

add_library(deqp-gles3-accuracy STATIC ${DEQP_GLES3_ACCURACY_SRCS})
target_link_libraries(deqp-gles3-accuracy deqp-gl-shared glutil tcutil ${DEQP_GLES3_LIBRARIES})
