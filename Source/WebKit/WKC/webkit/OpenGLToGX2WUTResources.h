/*
 * Native GX2R buffer storage for the OpenGL compatibility layer.
 *
 * This adapter owns GPU-visible buffer allocations and performs CPU-write
 * cache maintenance through GX2R. Vertex fetch layout remains the responsibility
 * of the fetch-shader/vertex-input adapter.
 */
#pragma once

#include "OpenGLToGX2Buffers.h"
#include "OpenGLToGX2VertexArrays.h"

#if defined(__WUT__)
#include <gx2r/buffer.h>
#include <unordered_map>

namespace WKC {
namespace GLGX2 {

struct WUTBufferResources {
    std::unordered_map<BufferName, GX2RBuffer*> buffers;
    std::unordered_map<uint8_t, BufferName> bindings;
};

BufferBackend makeWUTBufferBackend(WUTBufferResources&);
VertexArrayBackend makeWUTVertexArrayBackend(WUTBufferResources&);

} // namespace GLGX2
} // namespace WKC
#endif
