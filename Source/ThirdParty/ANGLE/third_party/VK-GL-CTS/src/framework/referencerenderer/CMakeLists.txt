
set(RR_SRCS
	rrDefs.cpp
	rrDefs.hpp
	rrFragmentOperations.cpp
	rrFragmentOperations.hpp
	rrFragmentPacket.hpp
	rrGenericVector.hpp
	rrMultisamplePixelBufferAccess.cpp
	rrMultisamplePixelBufferAccess.hpp
	rrPrimitiveAssembler.hpp
	rrPrimitivePacket.cpp
	rrPrimitivePacket.hpp
	rrPrimitiveTypes.hpp
	rrRasterizer.cpp
	rrRasterizer.hpp
	rrRenderer.cpp
	rrRenderer.hpp
	rrRenderState.hpp
	rrShaders.cpp
	rrShaders.hpp
	rrShadingContext.cpp
	rrShadingContext.hpp
	rrVertexAttrib.cpp
	rrVertexAttrib.hpp
	rrVertexPacket.cpp
	rrVertexPacket.hpp
	)

PCH(RR_SRCS ../pch.cpp)

add_library(referencerenderer STATIC ${RR_SRCS})
target_link_libraries(referencerenderer tcutil)
