/*
 * Current WebKit GraphicsContextGL backend for Wii U.
 * The WebGL 1 shader/draw path uses CafeGLSL and Nintendo GX2.
 * The old GraphicsContext3DWKC.cpp and its WKC entry points are kept intact.
 */

#include "config.h"

#if ENABLE(WEBGL)

#include "GraphicsContextGL.h"
#include "NotImplemented.h"
#include "OpenGLToGX2Buffers.h"
#include "OpenGLToGX2Driver.h"
#include "OpenGLToGX2VertexArrays.h"
#include "OpenGLToGX2WUTBackend.h"

#include <cafeglsl/CafeGLSLCompiler.h>
#include <gx2/draw.h>
#include <gx2/event.h>
#include <gx2/shaders.h>
#include <gx2/texture.h>
#include <gx2/sampler.h>
#include <gx2/utils.h>
#include <gx2r/surface.h>
#include <coreinit/cache.h>
#include <wtf/StdLibExtras.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace WebCore {

#if defined(__WUT__)

namespace {

constexpr GCGLenum GL_ARRAY_BUFFER_VALUE = 0x8892;
constexpr GCGLenum GL_ELEMENT_ARRAY_BUFFER_VALUE = 0x8893;
constexpr GCGLenum GL_VERTEX_SHADER_VALUE = 0x8B31;
constexpr GCGLenum GL_FRAGMENT_SHADER_VALUE = 0x8B30;
constexpr GCGLenum GL_COMPILE_STATUS_VALUE = 0x8B81;
constexpr GCGLenum GL_LINK_STATUS_VALUE = 0x8B82;
constexpr GCGLenum GL_VALIDATE_STATUS_VALUE = 0x8B83;
constexpr GCGLenum GL_INFO_LOG_LENGTH_VALUE = 0x8B84;
constexpr GCGLenum GL_ATTACHED_SHADERS_VALUE = 0x8B85;
constexpr GCGLenum GL_ACTIVE_UNIFORMS_VALUE = 0x8B86;
constexpr GCGLenum GL_ACTIVE_ATTRIBUTES_VALUE = 0x8B89;
constexpr GCGLenum GL_DELETE_STATUS_VALUE = 0x8B80;
constexpr GCGLenum GL_SHADER_TYPE_VALUE = 0x8B4F;
constexpr GCGLenum GL_SHADER_SOURCE_LENGTH_VALUE = 0x8B88;
constexpr GCGLenum GL_FLOAT_VALUE = 0x1406;
constexpr GCGLenum GL_INT_VALUE = 0x1404;
constexpr GCGLenum GL_UNSIGNED_INT_VALUE = 0x1405;
constexpr GCGLenum GL_BOOL_VALUE = 0x8B56;
constexpr GCGLenum GL_FLOAT_VEC2_VALUE = 0x8B50;
constexpr GCGLenum GL_FLOAT_VEC3_VALUE = 0x8B51;
constexpr GCGLenum GL_FLOAT_VEC4_VALUE = 0x8B52;
constexpr GCGLenum GL_INT_VEC2_VALUE = 0x8B53;
constexpr GCGLenum GL_INT_VEC3_VALUE = 0x8B54;
constexpr GCGLenum GL_INT_VEC4_VALUE = 0x8B55;
constexpr GCGLenum GL_BOOL_VEC2_VALUE = 0x8B57;
constexpr GCGLenum GL_BOOL_VEC3_VALUE = 0x8B58;
constexpr GCGLenum GL_BOOL_VEC4_VALUE = 0x8B59;
constexpr GCGLenum GL_FLOAT_MAT2_VALUE = 0x8B5A;
constexpr GCGLenum GL_FLOAT_MAT3_VALUE = 0x8B5B;
constexpr GCGLenum GL_FLOAT_MAT4_VALUE = 0x8B5C;
constexpr GCGLenum GL_TRIANGLES_VALUE = 0x0004;
constexpr GCGLenum GL_POINTS_VALUE = 0x0000;
constexpr GCGLenum GL_LINES_VALUE = 0x0001;
constexpr GCGLenum GL_LINE_STRIP_VALUE = 0x0003;
constexpr GCGLenum GL_TRIANGLE_STRIP_VALUE = 0x0005;
constexpr GCGLenum GL_TRIANGLE_FAN_VALUE = 0x0006;
constexpr GCGLenum GL_COLOR_BUFFER_BIT_VALUE = 0x00004000;
constexpr GCGLenum GL_DEPTH_BUFFER_BIT_VALUE = 0x00000100;
constexpr GCGLenum GL_STENCIL_BUFFER_BIT_VALUE = 0x00000400;
constexpr GCGLenum GL_FRAMEBUFFER_VALUE = 0x8D40;
constexpr GCGLenum GL_FRAMEBUFFER_COMPLETE_VALUE = 0x8CD5;
constexpr GCGLenum GL_FRAMEBUFFER_UNSUPPORTED_VALUE = 0x8CDD;
constexpr GCGLenum GL_FRAMEBUFFER_BINDING_VALUE = 0x8CA6;
constexpr GCGLenum GL_ARRAY_BUFFER_BINDING_VALUE = 0x8894;
constexpr GCGLenum GL_ELEMENT_ARRAY_BUFFER_BINDING_VALUE = 0x8895;
constexpr GCGLenum GL_BUFFER_SIZE_VALUE = 0x8764;
constexpr GCGLenum GL_BUFFER_USAGE_VALUE = 0x8765;
constexpr GCGLenum GL_STATIC_DRAW_VALUE = 0x88E4;
constexpr GCGLenum GL_DYNAMIC_DRAW_VALUE = 0x88E8;
constexpr GCGLenum GL_STREAM_DRAW_VALUE = 0x88E0;
constexpr GCGLenum GL_BYTE_VALUE = 0x1400;
constexpr GCGLenum GL_UNSIGNED_BYTE_VALUE = 0x1401;
constexpr GCGLenum GL_SHORT_VALUE = 0x1402;
constexpr GCGLenum GL_UNSIGNED_SHORT_VALUE = 0x1403;
constexpr GCGLenum GL_HALF_FLOAT_VALUE = 0x140B;
constexpr GCGLenum GL_BLEND_VALUE = 0x0BE2;
constexpr GCGLenum GL_CULL_FACE_VALUE = 0x0B44;
constexpr GCGLenum GL_DEPTH_TEST_VALUE = 0x0B71;
constexpr GCGLenum GL_SCISSOR_TEST_VALUE = 0x0C11;
constexpr GCGLenum GL_STENCIL_TEST_VALUE = 0x0B90;
constexpr GCGLenum GL_POLYGON_OFFSET_FILL_VALUE = 0x8037;
constexpr GCGLenum GL_INVALID_ENUM_VALUE = 0x0500;
constexpr GCGLenum GL_INVALID_VALUE_VALUE = 0x0501;
constexpr GCGLenum GL_INVALID_OPERATION_VALUE = 0x0502;
constexpr GCGLenum GL_OUT_OF_MEMORY_VALUE = 0x0505;
constexpr GCGLenum GL_VENDOR_VALUE = 0x1F00;
constexpr GCGLenum GL_RENDERER_VALUE = 0x1F01;
constexpr GCGLenum GL_VERSION_VALUE = 0x1F02;
constexpr GCGLenum GL_SHADING_LANGUAGE_VERSION_VALUE = 0x8B8C;
constexpr GCGLenum GL_EXTENSIONS_VALUE = 0x1F03;
constexpr GCGLenum GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_VALUE = 0x8B4D;
constexpr GCGLenum GL_MAX_VERTEX_ATTRIBS_VALUE = 0x8869;
constexpr GCGLenum GL_MAX_TEXTURE_SIZE_VALUE = 0x0D33;
constexpr GCGLenum GL_MAX_CUBE_MAP_TEXTURE_SIZE_VALUE = 0x851C;
constexpr GCGLenum GL_MAX_RENDERBUFFER_SIZE_VALUE = 0x84E8;
constexpr GCGLenum GL_VIEWPORT_VALUE = 0x0BA2;
constexpr GCGLenum GL_COLOR_CLEAR_VALUE_VALUE = 0x0C22;
constexpr GCGLenum GL_DEPTH_CLEAR_VALUE_VALUE = 0x0B73;
constexpr GCGLenum GL_DEPTH_RANGE_VALUE = 0x0B70;
constexpr GCGLenum GL_TEXTURE_2D_VALUE = 0x0DE1;
constexpr GCGLenum GL_TEXTURE_MIN_FILTER_VALUE = 0x2801;
constexpr GCGLenum GL_TEXTURE_MAG_FILTER_VALUE = 0x2800;
constexpr GCGLenum GL_TEXTURE_WRAP_S_VALUE = 0x2802;
constexpr GCGLenum GL_TEXTURE_WRAP_T_VALUE = 0x2803;
constexpr GCGLenum GL_TEXTURE_BINDING_2D_VALUE = 0x8069;
constexpr GCGLenum GL_UNPACK_ALIGNMENT_VALUE = 0x0CF5;
constexpr GCGLenum GL_PACK_ALIGNMENT_VALUE = 0x0D05;
constexpr GCGLenum GL_RGB_VALUE = 0x1907;
constexpr GCGLenum GL_RGBA_VALUE = 0x1908;
constexpr GCGLenum GL_ALPHA_VALUE = 0x1906;
constexpr GCGLenum GL_LUMINANCE_VALUE = 0x1909;
constexpr GCGLenum GL_LUMINANCE_ALPHA_VALUE = 0x190A;
constexpr GCGLenum GL_TEXTURE0_VALUE = 0x84C0;
constexpr GCGLenum GL_NEAREST_VALUE = 0x2600;
constexpr GCGLenum GL_LINEAR_VALUE = 0x2601;
constexpr GCGLenum GL_NEAREST_MIPMAP_NEAREST_VALUE = 0x2700;
constexpr GCGLenum GL_LINEAR_MIPMAP_NEAREST_VALUE = 0x2701;
constexpr GCGLenum GL_NEAREST_MIPMAP_LINEAR_VALUE = 0x2702;
constexpr GCGLenum GL_LINEAR_MIPMAP_LINEAR_VALUE = 0x2703;
constexpr GCGLenum GL_REPEAT_VALUE = 0x2901;
constexpr GCGLenum GL_CLAMP_TO_EDGE_VALUE = 0x812F;
constexpr GCGLenum GL_MIRRORED_REPEAT_VALUE = 0x8370;
constexpr GCGLenum GL_SAMPLER_2D_VALUE = 0x8B5E;
constexpr GCGLenum GL_SAMPLER_CUBE_VALUE = 0x8B60;

constexpr GX2RResourceFlags textureSurfaceFlags = static_cast<GX2RResourceFlags>(
    GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_CPU_WRITE
    | GX2R_RESOURCE_USAGE_CPU_READ | GX2R_RESOURCE_USAGE_GPU_READ);

void initializeGLSLCompilerOnce()
{
    static bool initialized = false;
    if (!initialized) {
        InitGLSLCompiler();
        initialized = true;
    }
}

bool mapBufferTarget(GCGLenum target, WKC::GLGX2::BufferTarget& output)
{
    using WKC::GLGX2::BufferTarget;
    switch (target) {
    case GL_ARRAY_BUFFER_VALUE: output = BufferTarget::Array; return true;
    case GL_ELEMENT_ARRAY_BUFFER_VALUE: output = BufferTarget::ElementArray; return true;
    default: return false;
    }
}

bool mapPrimitive(GCGLenum mode, WKC::GLGX2::Primitive& output)
{
    using WKC::GLGX2::Primitive;
    switch (mode) {
    case GL_POINTS_VALUE: output = Primitive::Points; return true;
    case GL_LINES_VALUE: output = Primitive::Lines; return true;
    case GL_LINE_STRIP_VALUE: output = Primitive::LineStrip; return true;
    case GL_TRIANGLES_VALUE: output = Primitive::Triangles; return true;
    case GL_TRIANGLE_STRIP_VALUE: output = Primitive::TriangleStrip; return true;
    case GL_TRIANGLE_FAN_VALUE: output = Primitive::TriangleFan; return true;
    default: return false;
    }
}

bool mapIndexType(GCGLenum type, WKC::GLGX2::IndexType& output)
{
    using WKC::GLGX2::IndexType;
    switch (type) {
    case GL_UNSIGNED_BYTE_VALUE: output = IndexType::UnsignedByte; return true;
    case GL_UNSIGNED_SHORT_VALUE: output = IndexType::UnsignedShort; return true;
    case GL_UNSIGNED_INT_VALUE: output = IndexType::UnsignedInt; return true;
    default: return false;
    }
}

bool mapAttributeType(GCGLenum type, WKC::GLGX2::AttributeType& output)
{
    using WKC::GLGX2::AttributeType;
    switch (type) {
    case GL_BYTE_VALUE: output = AttributeType::Byte; return true;
    case GL_UNSIGNED_BYTE_VALUE: output = AttributeType::UnsignedByte; return true;
    case GL_SHORT_VALUE: output = AttributeType::Short; return true;
    case GL_UNSIGNED_SHORT_VALUE: output = AttributeType::UnsignedShort; return true;
    case GL_INT_VALUE: output = AttributeType::Int; return true;
    case GL_UNSIGNED_INT_VALUE: output = AttributeType::UnsignedInt; return true;
    case GL_FLOAT_VALUE: output = AttributeType::Float; return true;
    case GL_HALF_FLOAT_VALUE: output = AttributeType::HalfFloat; return true;
    default: return false;
    }
}

GCGLenum mapGX2VariableType(GX2ShaderVarType type)
{
    switch (type) {
    case GX2_SHADER_VAR_TYPE_FLOAT: return GL_FLOAT_VALUE;
    case GX2_SHADER_VAR_TYPE_FLOAT2: return GL_FLOAT_VEC2_VALUE;
    case GX2_SHADER_VAR_TYPE_FLOAT3: return GL_FLOAT_VEC3_VALUE;
    case GX2_SHADER_VAR_TYPE_FLOAT4: return GL_FLOAT_VEC4_VALUE;
    case GX2_SHADER_VAR_TYPE_INT: return GL_INT_VALUE;
    case GX2_SHADER_VAR_TYPE_INT2: return GL_INT_VEC2_VALUE;
    case GX2_SHADER_VAR_TYPE_INT3: return GL_INT_VEC3_VALUE;
    case GX2_SHADER_VAR_TYPE_INT4: return GL_INT_VEC4_VALUE;
    case GX2_SHADER_VAR_TYPE_UINT: return GL_UNSIGNED_INT_VALUE;
    case GX2_SHADER_VAR_TYPE_BOOL: return GL_BOOL_VALUE;
    case GX2_SHADER_VAR_TYPE_BOOL2: return GL_BOOL_VEC2_VALUE;
    case GX2_SHADER_VAR_TYPE_BOOL3: return GL_BOOL_VEC3_VALUE;
    case GX2_SHADER_VAR_TYPE_BOOL4: return GL_BOOL_VEC4_VALUE;
    case GX2_SHADER_VAR_TYPE_FLOAT2X2: return GL_FLOAT_MAT2_VALUE;
    case GX2_SHADER_VAR_TYPE_FLOAT3X3: return GL_FLOAT_MAT3_VALUE;
    case GX2_SHADER_VAR_TYPE_FLOAT4X4: return GL_FLOAT_MAT4_VALUE;
    default: return 0;
    }
}

uint32_t uniformComponents(GCGLenum type)
{
    switch (type) {
    case GL_FLOAT_VEC2_VALUE: case GL_INT_VEC2_VALUE: case GL_BOOL_VEC2_VALUE: return 2;
    case GL_FLOAT_VEC3_VALUE: case GL_INT_VEC3_VALUE: case GL_BOOL_VEC3_VALUE: return 3;
    case GL_FLOAT_VEC4_VALUE: case GL_INT_VEC4_VALUE: case GL_BOOL_VEC4_VALUE: return 4;
    case GL_FLOAT_MAT2_VALUE: return 4;
    case GL_FLOAT_MAT3_VALUE: return 9;
    case GL_FLOAT_MAT4_VALUE: return 16;
    default: return 1;
    }
}

uint32_t uniformRegisters(GCGLenum type)
{
    switch (type) {
    case GL_FLOAT_MAT2_VALUE: return 2;
    case GL_FLOAT_MAT3_VALUE: return 3;
    case GL_FLOAT_MAT4_VALUE: return 4;
    default: return 1;
    }
}

} // namespace
#endif // __WUT__

#if defined(__WUT__)

class GraphicsContextGLWKC final : public GraphicsContextGL {
public:
    explicit GraphicsContextGLWKC(const GraphicsContextGLAttributes&);
    ~GraphicsContextGLWKC() override;
    bool initialize();

    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() override;

    void activeTexture(GCGLenum texture) override;

    void attachShader(PlatformGLObject program, PlatformGLObject shader) override;

    void bindAttribLocation(PlatformGLObject, GCGLuint index, const CString& name) override;

    void bindBuffer(GCGLenum target, PlatformGLObject) override;

    void bindFramebuffer(GCGLenum target, PlatformGLObject) override;

    void bindRenderbuffer(GCGLenum target, PlatformGLObject) override { notImplemented(); }

    void bindTexture(GCGLenum target, PlatformGLObject) override;

    void blendColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha) override;

    void blendEquation(GCGLenum mode) override;

    void blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha) override;

    void blendFunc(GCGLenum sfactor, GCGLenum dfactor) override;

    void blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha) override;

    GCGLenum checkFramebufferStatus(GCGLenum target) override;

    void clear(GCGLbitfield mask) override;

    void clearColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha) override;

    void clearDepth(GCGLclampf depth) override;

    void clearStencil(GCGLint s) override;

    void colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha) override;

    void compileShader(PlatformGLObject) override;

    void copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border) override { notImplemented(); }

    void copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) override { notImplemented(); }

    PlatformGLObject createBuffer() override;

    PlatformGLObject createFramebuffer() override { notImplemented(); return {}; }

    PlatformGLObject createProgram() override;

    PlatformGLObject createRenderbuffer() override { notImplemented(); return {}; }

    PlatformGLObject createShader(GCGLenum) override;

    PlatformGLObject createTexture() override;

    void cullFace(GCGLenum mode) override;

    void deleteBuffer(PlatformGLObject) override;

    void deleteFramebuffer(PlatformGLObject) override { notImplemented(); }

    void deleteProgram(PlatformGLObject) override;

    void deleteRenderbuffer(PlatformGLObject) override { notImplemented(); }

    void deleteShader(PlatformGLObject) override;

    void deleteTexture(PlatformGLObject) override;

    void depthFunc(GCGLenum func) override;

    void depthMask(GCGLboolean flag) override;

    void depthRange(GCGLclampf zNear, GCGLclampf zFar) override;

    void detachShader(PlatformGLObject, PlatformGLObject) override;

    void disable(GCGLenum cap) override;

    void disableVertexAttribArray(GCGLuint index) override;

    void drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count) override;

    void drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset) override;

    void enable(GCGLenum cap) override;

    void enableVertexAttribArray(GCGLuint index) override;

    void finish() override;

    void flush() override;

    void framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, PlatformGLObject) override { notImplemented(); }

    void framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, PlatformGLObject, GCGLint level) override { notImplemented(); }

    void frontFace(GCGLenum mode) override;

    void generateMipmap(GCGLenum target) override;

    Vector<GCGLAttribActiveInfo> activeAttribs(PlatformGLObject program) override;

    Vector<GCGLUniformActiveInfo> activeUniforms(PlatformGLObject program) override;

    GCGLint getBufferParameteri(GCGLenum target, GCGLenum pname) override;

    CString getString(GCGLenum name) override;

    void getFloatv(GCGLenum pname, std::span<GCGLfloat> value) override;

    void getIntegerv(GCGLenum pname, std::span<GCGLint> value) override;

    void getIntegeri_v(GCGLenum pname, GCGLuint index, std::span<GCGLint, 4> value) override { notImplemented(); }

    GCGLint64 getInteger64(GCGLenum pname) override { notImplemented(); return {}; }

    GCGLint64 getInteger64i(GCGLenum pname, GCGLuint index) override { notImplemented(); return {}; }

    GCGLint getProgrami(PlatformGLObject program, GCGLenum pname) override;

    void getBooleanv(GCGLenum pname, std::span<GCGLboolean> value) override { notImplemented(); }

    GCGLErrorCodeSet getErrors() override;

    GCGLint getFramebufferAttachmentParameteri(GCGLenum target, GCGLenum attachment, GCGLenum pname) override { notImplemented(); return {}; }

    CString getProgramInfoLog(PlatformGLObject) override;

    GCGLint getRenderbufferParameteri(GCGLenum target, GCGLenum pname) override { notImplemented(); return {}; }

    GCGLint getShaderi(PlatformGLObject, GCGLenum pname) override;

    CString getShaderInfoLog(PlatformGLObject) override;

    void getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, std::span<GCGLint, 2> range, GCGLint* precision) override { notImplemented(); }

    GCGLfloat getTexParameterf(GCGLenum target, GCGLenum pname) override;

    GCGLint getTexParameteri(GCGLenum target, GCGLenum pname) override;

    void getUniformfv(PlatformGLObject program, GCGLint location, std::span<GCGLfloat> value) override;

    void getUniformiv(PlatformGLObject program, GCGLint location, std::span<GCGLint> value) override;

    void getUniformuiv(PlatformGLObject program, GCGLint location, std::span<GCGLuint> value) override;

    GCGLsizeiptr getVertexAttribOffset(GCGLuint index, GCGLenum pname) override;

    void hint(GCGLenum target, GCGLenum mode) override { notImplemented(); }

    GCGLboolean isBuffer(PlatformGLObject) override;

    GCGLboolean isEnabled(GCGLenum cap) override;

    GCGLboolean isFramebuffer(PlatformGLObject) override { notImplemented(); return {}; }

    GCGLboolean isProgram(PlatformGLObject) override;

    GCGLboolean isRenderbuffer(PlatformGLObject) override { notImplemented(); return {}; }

    GCGLboolean isShader(PlatformGLObject) override;

    GCGLboolean isTexture(PlatformGLObject) override;

    void lineWidth(GCGLfloat) override { notImplemented(); }

    void linkProgram(PlatformGLObject) override;

    void pixelStorei(GCGLenum pname, GCGLint param) override;

    void polygonOffset(GCGLfloat factor, GCGLfloat units) override;

    void renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) override { notImplemented(); }

    void sampleCoverage(GCGLclampf value, GCGLboolean invert) override { notImplemented(); }

    void scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) override;

    void shaderSource(PlatformGLObject shader, const CString& source) override;

    void stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask) override { notImplemented(); }

    void stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask) override { notImplemented(); }

    void stencilMask(GCGLuint mask) override { notImplemented(); }

    void stencilMaskSeparate(GCGLenum face, GCGLuint mask) override { notImplemented(); }

    void stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass) override { notImplemented(); }

    void stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass) override { notImplemented(); }

    void texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat param) override;

    void texParameteri(GCGLenum target, GCGLenum pname, GCGLint param) override;

    void uniform1f(GCGLint location, GCGLfloat x) override;

    void uniform1fv(GCGLint location, std::span<const GCGLfloat> v) override;

    void uniform1i(GCGLint location, GCGLint x) override;

    void uniform1iv(GCGLint location, std::span<const GCGLint> v) override;

    void uniform2f(GCGLint location, GCGLfloat x, GCGLfloat y) override;

    void uniform2fv(GCGLint location, std::span<const GCGLfloat> v) override;

    void uniform2i(GCGLint location, GCGLint x, GCGLint y) override;

    void uniform2iv(GCGLint location, std::span<const GCGLint> v) override;

    void uniform3f(GCGLint location, GCGLfloat x, GCGLfloat y, GCGLfloat z) override;

    void uniform3fv(GCGLint location, std::span<const GCGLfloat> v) override;

    void uniform3i(GCGLint location, GCGLint x, GCGLint y, GCGLint z) override;

    void uniform3iv(GCGLint location, std::span<const GCGLint> v) override;

    void uniform4f(GCGLint location, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w) override;

    void uniform4fv(GCGLint location, std::span<const GCGLfloat> v) override;

    void uniform4i(GCGLint location, GCGLint x, GCGLint y, GCGLint z, GCGLint w) override;

    void uniform4iv(GCGLint location, std::span<const GCGLint> v) override;

    void uniformMatrix2fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> value) override;

    void uniformMatrix3fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> value) override;

    void uniformMatrix4fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> value) override;

    void useProgram(PlatformGLObject) override;

    void validateProgram(PlatformGLObject) override;

    void vertexAttrib1f(GCGLuint index, GCGLfloat x) override { notImplemented(); }

    void vertexAttrib1fv(GCGLuint index, std::span<const GCGLfloat, 1> values) override { notImplemented(); }

    void vertexAttrib2f(GCGLuint index, GCGLfloat x, GCGLfloat y) override { notImplemented(); }

    void vertexAttrib2fv(GCGLuint index, std::span<const GCGLfloat, 2> values) override { notImplemented(); }

    void vertexAttrib3f(GCGLuint index, GCGLfloat x, GCGLfloat y, GCGLfloat z) override { notImplemented(); }

    void vertexAttrib3fv(GCGLuint index, std::span<const GCGLfloat, 3> values) override { notImplemented(); }

    void vertexAttrib4f(GCGLuint index, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w) override { notImplemented(); }

    void vertexAttrib4fv(GCGLuint index, std::span<const GCGLfloat, 4> values) override { notImplemented(); }

    void vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, GCGLintptr offset) override;

    void viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) override;

    void bufferData(GCGLenum target, GCGLsizeiptr, GCGLenum usage) override;

    void bufferData(GCGLenum target, std::span<const uint8_t> data, GCGLenum usage) override;

    void bufferSubData(GCGLenum target, GCGLintptr offset, std::span<const uint8_t> data) override;

    void readPixels(IntRect, GCGLenum format, GCGLenum type, std::span<uint8_t> data, GCGLint packAlignment, GCGLint packRowLength, GCGLboolean packReverseRowOrder) override { notImplemented(); }

    void readPixelsBufferObject(IntRect, GCGLenum format, GCGLenum type, GCGLintptr offset, GCGLint alignment, GCGLint rowLength) override { notImplemented(); }

    void texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels) override;

    void texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset) override { notImplemented(); }

    void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels) override;

    void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset) override { notImplemented(); }

    void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, std::span<const uint8_t> data) override { notImplemented(); }

    void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, GCGLintptr offset) override { notImplemented(); }

    void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, std::span<const uint8_t> data) override { notImplemented(); }

    void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset) override { notImplemented(); }

    void drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount) override;

    void drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei primcount) override;

    void vertexAttribDivisor(GCGLuint index, GCGLuint divisor) override;

    PlatformGLObject createVertexArray() override;

    void deleteVertexArray(PlatformGLObject) override;

    GCGLboolean isVertexArray(PlatformGLObject) override;

    void bindVertexArray(PlatformGLObject) override;

    void copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLintptr readOffset, GCGLintptr writeOffset, GCGLsizeiptr size) override { notImplemented(); }

    void getBufferSubData(GCGLenum target, GCGLintptr offset, std::span<uint8_t> data) override { notImplemented(); }

    void blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter) override { notImplemented(); }

    void framebufferTextureLayer(GCGLenum target, GCGLenum attachment, PlatformGLObject texture, GCGLint level, GCGLint layer) override { notImplemented(); }

    void invalidateFramebuffer(GCGLenum target, std::span<const GCGLenum> attachments) override { notImplemented(); }

    void invalidateSubFramebuffer(GCGLenum target, std::span<const GCGLenum> attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) override { notImplemented(); }

    void readBuffer(GCGLenum src) override { notImplemented(); }

    void getInternalformativ(GCGLenum target, GCGLenum internalformat, GCGLenum pname, std::span<GCGLint> data) override { notImplemented(); }

    void renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) override { notImplemented(); }

    void texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) override { notImplemented(); }

    void texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth) override { notImplemented(); }

    void texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels) override { notImplemented(); }

    void texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset) override { notImplemented(); }

    void texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels) override { notImplemented(); }

    void texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLintptr offset) override { notImplemented(); }

    void copyTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) override { notImplemented(); }

    void compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, std::span<const uint8_t> data) override { notImplemented(); }

    void compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLsizei imageSize, GCGLintptr offset) override { notImplemented(); }

    void compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, std::span<const uint8_t> data) override { notImplemented(); }

    void compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset) override { notImplemented(); }

    GCGLint getFragDataLocation(PlatformGLObject program, const CString& name) override { notImplemented(); return {}; }

    void uniform1ui(GCGLint location, GCGLuint v0) override { notImplemented(); }

    void uniform2ui(GCGLint location, GCGLuint v0, GCGLuint v1) override { notImplemented(); }

    void uniform3ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2) override { notImplemented(); }

    void uniform4ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2, GCGLuint v3) override { notImplemented(); }

    void uniform1uiv(GCGLint location, std::span<const GCGLuint> data) override { notImplemented(); }

    void uniform2uiv(GCGLint location, std::span<const GCGLuint> data) override { notImplemented(); }

    void uniform3uiv(GCGLint location, std::span<const GCGLuint> data) override { notImplemented(); }

    void uniform4uiv(GCGLint location, std::span<const GCGLuint> data) override { notImplemented(); }

    void uniformMatrix2x3fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data) override { notImplemented(); }

    void uniformMatrix3x2fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data) override { notImplemented(); }

    void uniformMatrix2x4fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data) override { notImplemented(); }

    void uniformMatrix4x2fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data) override { notImplemented(); }

    void uniformMatrix3x4fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data) override { notImplemented(); }

    void uniformMatrix4x3fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data) override { notImplemented(); }

    void vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w) override { notImplemented(); }

    void vertexAttribI4iv(GCGLuint index, std::span<const GCGLint, 4> values) override { notImplemented(); }

    void vertexAttribI4ui(GCGLuint index, GCGLuint x, GCGLuint y, GCGLuint z, GCGLuint w) override { notImplemented(); }

    void vertexAttribI4uiv(GCGLuint index, std::span<const GCGLuint, 4> values) override { notImplemented(); }

    void vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLintptr offset) override { notImplemented(); }

    void drawRangeElements(GCGLenum mode, GCGLuint start, GCGLuint end, GCGLsizei count, GCGLenum type, GCGLintptr offset) override { notImplemented(); }

    void drawBuffers(std::span<const GCGLenum> bufs) override { notImplemented(); }

    void clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, std::span<const GCGLint> values) override { notImplemented(); }

    void clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, std::span<const GCGLuint> values) override { notImplemented(); }

    void clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, std::span<const GCGLfloat> values) override { notImplemented(); }

    void clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat depth, GCGLint stencil) override { notImplemented(); }

    PlatformGLObject createQuery() override { notImplemented(); return {}; }

    void deleteQuery(PlatformGLObject query) override { notImplemented(); }

    GCGLboolean isQuery(PlatformGLObject query) override { notImplemented(); return {}; }

    void beginQuery(GCGLenum target, PlatformGLObject query) override { notImplemented(); }

    void endQuery(GCGLenum target) override { notImplemented(); }

    GCGLint getQuery(GCGLenum target, GCGLenum pname) override { notImplemented(); return {}; }

    GCGLuint getQueryObjectui(PlatformGLObject query, GCGLenum pname) override { notImplemented(); return {}; }

    PlatformGLObject createSampler() override { notImplemented(); return {}; }

    void deleteSampler(PlatformGLObject sampler) override { notImplemented(); }

    GCGLboolean isSampler(PlatformGLObject sampler) override { notImplemented(); return {}; }

    void bindSampler(GCGLuint unit, PlatformGLObject sampler) override { notImplemented(); }

    void samplerParameteri(PlatformGLObject sampler, GCGLenum pname, GCGLint param) override { notImplemented(); }

    void samplerParameterf(PlatformGLObject sampler, GCGLenum pname, GCGLfloat param) override { notImplemented(); }

    GCGLfloat getSamplerParameterf(PlatformGLObject sampler, GCGLenum pname) override { notImplemented(); return {}; }

    GCGLint getSamplerParameteri(PlatformGLObject sampler, GCGLenum pname) override { notImplemented(); return {}; }

    GCGLsync fenceSync(GCGLenum condition, GCGLbitfield flags) override { notImplemented(); return {}; }

    GCGLboolean isSync(GCGLsync) override { notImplemented(); return {}; }

    void deleteSync(GCGLsync) override { notImplemented(); }

    GCGLenum clientWaitSync(GCGLsync, GCGLbitfield flags, GCGLuint64 timeout) override { notImplemented(); return {}; }

    void waitSync(GCGLsync, GCGLbitfield flags, GCGLint64 timeout) override { notImplemented(); }

    GCGLint getSynci(GCGLsync, GCGLenum pname) override { notImplemented(); return {}; }

    PlatformGLObject createTransformFeedback() override { notImplemented(); return {}; }

    void deleteTransformFeedback(PlatformGLObject id) override { notImplemented(); }

    GCGLboolean isTransformFeedback(PlatformGLObject id) override { notImplemented(); return {}; }

    void bindTransformFeedback(GCGLenum target, PlatformGLObject id) override { notImplemented(); }

    void beginTransformFeedback(GCGLenum primitiveMode) override { notImplemented(); }

    void endTransformFeedback() override { notImplemented(); }

    void transformFeedbackVaryings(PlatformGLObject program, const Vector<CString>& varyings, GCGLenum bufferMode) override { notImplemented(); }

    std::optional<GCGLTransformFeedbackActiveInfo> getTransformFeedbackVarying(PlatformGLObject program, GCGLuint index) override { notImplemented(); return {}; }

    void pauseTransformFeedback() override { notImplemented(); }

    void resumeTransformFeedback() override { notImplemented(); }

    void bindBufferBase(GCGLenum target, GCGLuint index, PlatformGLObject buffer) override { notImplemented(); }

    void bindBufferRange(GCGLenum target, GCGLuint index, PlatformGLObject buffer, GCGLintptr offset, GCGLsizeiptr size) override { notImplemented(); }

    GCGLuint getUniformBlockIndex(PlatformGLObject program, const CString& uniformBlockName) override { notImplemented(); return {}; }

    CString getActiveUniformBlockName(PlatformGLObject program, GCGLuint uniformBlockIndex) override { notImplemented(); return {}; }

    void uniformBlockBinding(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding) override { notImplemented(); }

    void getActiveUniformBlockiv(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLenum pname, std::span<GCGLint> params) override { notImplemented(); }

#if (ENABLE(WEBXR))
    bool addFoveation(IntSize, IntSize, IntSize, std::span<const GCGLfloat>, std::span<const GCGLfloat>, std::span<const GCGLfloat>) override { notImplemented(); return {}; }
#endif

#if (ENABLE(WEBXR))
    void enableFoveation(GCGLuint) override { notImplemented(); }
#endif

#if (ENABLE(WEBXR))
    void disableFoveation() override { notImplemented(); }
#endif

#if (ENABLE(WEBXR))
    GCGLExternalImage createExternalImage(ExternalImageSource&&, GCGLenum internalFormat, GCGLint layer) override { notImplemented(); return {}; }
#endif

#if (ENABLE(WEBXR))
    void deleteExternalImage(GCGLExternalImage) override { notImplemented(); }
#endif

#if (ENABLE(WEBXR))
    void bindExternalImage(GCGLenum target, GCGLExternalImage) override { notImplemented(); }
#endif

#if (ENABLE(WEBXR))
    GCGLExternalSync createExternalSync(ExternalSyncSource&&) override { notImplemented(); return {}; }
#endif

#if (ENABLE(WEBXR))
    void deleteExternalSync(GCGLExternalSync) override { notImplemented(); }
#endif

    void multiDrawArraysANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei> firstsAndCounts) override { notImplemented(); }

    void multiDrawArraysInstancedANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei, const GCGLsizei> firstsCountsAndInstanceCounts) override { notImplemented(); }

    void multiDrawElementsANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei> countsAndOffsets, GCGLenum type) override { notImplemented(); }

    void multiDrawElementsInstancedANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei, const GCGLsizei> countsOffsetsAndInstanceCounts, GCGLenum type) override { notImplemented(); }

    bool enableExtension(GCGLExtension) override { notImplemented(); return {}; }

    CString getTranslatedShaderSourceANGLE(PlatformGLObject) override { notImplemented(); return {}; }

    void drawBuffersEXT(std::span<const GCGLenum> bufs) override { notImplemented(); }

    PlatformGLObject createQueryEXT() override { notImplemented(); return {}; }

    void deleteQueryEXT(PlatformGLObject query) override { notImplemented(); }

    GCGLboolean isQueryEXT(PlatformGLObject query) override { notImplemented(); return {}; }

    void beginQueryEXT(GCGLenum target, PlatformGLObject query) override { notImplemented(); }

    void endQueryEXT(GCGLenum target) override { notImplemented(); }

    void queryCounterEXT(PlatformGLObject query, GCGLenum target) override { notImplemented(); }

    GCGLint getQueryiEXT(GCGLenum target, GCGLenum pname) override { notImplemented(); return {}; }

    GCGLint getQueryObjectiEXT(PlatformGLObject query, GCGLenum pname) override { notImplemented(); return {}; }

    GCGLuint64 getQueryObjectui64EXT(PlatformGLObject query, GCGLenum pname) override { notImplemented(); return {}; }

    GCGLint64 getInteger64EXT(GCGLenum pname) override { notImplemented(); return {}; }

    void enableiOES(GCGLenum target, GCGLuint index) override { notImplemented(); }

    void disableiOES(GCGLenum target, GCGLuint index) override { notImplemented(); }

    void blendEquationiOES(GCGLuint buf, GCGLenum mode) override { notImplemented(); }

    void blendEquationSeparateiOES(GCGLuint buf, GCGLenum modeRGB, GCGLenum modeAlpha) override { notImplemented(); }

    void blendFunciOES(GCGLuint buf, GCGLenum src, GCGLenum dst) override { notImplemented(); }

    void blendFuncSeparateiOES(GCGLuint buf, GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha) override { notImplemented(); }

    void colorMaskiOES(GCGLuint buf, GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha) override { notImplemented(); }

    void drawArraysInstancedBaseInstanceANGLE(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei instanceCount, GCGLuint baseInstance) override { notImplemented(); }

    void drawElementsInstancedBaseVertexBaseInstanceANGLE(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei instanceCount, GCGLint baseVertex, GCGLuint baseInstance) override { notImplemented(); }

    void multiDrawArraysInstancedBaseInstanceANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei, const GCGLsizei, const GCGLuint> firstsCountsInstanceCountsAndBaseInstances) override { notImplemented(); }

    void multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei, const GCGLsizei, const GCGLint, const GCGLuint> countsOffsetsInstanceCountsBaseVerticesAndBaseInstances, GCGLenum type) override { notImplemented(); }

    void clipControlEXT(GCGLenum origin, GCGLenum depth) override { notImplemented(); }

    void provokingVertexANGLE(GCGLenum provokeMode) override { notImplemented(); }

    void polygonModeANGLE(GCGLenum face, GCGLenum mode) override { notImplemented(); }

    void polygonOffsetClampEXT(GCGLfloat factor, GCGLfloat units, GCGLfloat clamp) override { notImplemented(); }

    void renderbufferStorageMultisampleANGLE(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) override { notImplemented(); }

    GCGLint maxCombinedTextureImageUnits() override;

    GCGLint maxVertexAttribs() override;

    GCGLint maxTextureSize() override;

    GCGLint maxCubeMapTextureSize() override;

    GCGLint maxRenderbufferSize() override;

    std::array<GCGLint, 2> maxViewportDims() override;

    GCGLint maxSamples() override;

    GCGLint maxTransformFeedbackSeparateAttribs() override;

    GCGLint maxUniformBufferBindings() override;

    GCGLint uniformBufferOffsetAlignment() override;

    GCGLint max3DTextureSize() override;

    GCGLint maxArrayTextureLayers() override;

    void reshape(int width, int height) override;

    void prepareForDisplay() override;

    RefPtr<NativeImage> copyNativeImageYFlipped(SurfaceBuffer) override { notImplemented(); return {}; }

#if (ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS))
    RefPtr<VideoFrame> surfaceBufferToVideoFrame(SurfaceBuffer) override { notImplemented(); return {}; }
#endif

    void simulateEventForTesting(SimulatedEventForTesting) override { notImplemented(); }

#if (ENABLE(VIDEO))
    bool copyTextureFromVideoFrame(VideoFrame&, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum  format, GCGLenum type, bool premultiplyAlpha, bool flipY) override { notImplemented(); return {}; }
#endif

private:
    struct ShaderRecord {
        GCGLenum type { 0 };
        std::string source;
        std::string log;
        GX2VertexShader* compiledVertex { nullptr };
        GX2PixelShader* compiledPixel { nullptr };
        GX2ShaderMode compiledMode { GLSL_SHADER_MODE_ERROR };
        bool compiled { false };
        bool deletePending { false };
    };
    struct AttributeRecord {
        std::string name;
        GCGLuint location { 0 };
        GCGLenum type { 0 };
        GCGLint size { 1 };
    };
    struct TextureRecord {
        GCGLenum target { 0 };
        GX2Texture native {};
        bool storageCreated { false };
        uint32_t width { 0 };
        uint32_t height { 0 };
        std::vector<std::vector<uint8_t>> levels;
        std::vector<bool> definedLevels;
        GX2Sampler sampler {};
        GCGLint minFilter { static_cast<GCGLint>(GL_NEAREST_MIPMAP_LINEAR_VALUE) };
        GCGLint magFilter { static_cast<GCGLint>(GL_LINEAR_VALUE) };
        GCGLint wrapS { static_cast<GCGLint>(GL_REPEAT_VALUE) };
        GCGLint wrapT { static_cast<GCGLint>(GL_REPEAT_VALUE) };
    };
    struct UniformBinding {
        GCGLint location { -1 };
        const GX2UniformVar* vertexVar { nullptr };
        const GX2UniformVar* pixelVar { nullptr };
        const GX2SamplerVar* vertexSamplerVar { nullptr };
        const GX2SamplerVar* pixelSamplerVar { nullptr };
        std::vector<uint8_t> value;
        bool valueSet { false };
        uint32_t arrayElement { 0 };
    };
    struct UniformRecord {
        std::string name;
        GCGLenum type { 0 };
        GCGLint size { 1 };
        std::vector<UniformBinding> elements;
        std::vector<GCGLint> samplerUnits;
    };
    struct UniformLocationInfo {
        size_t uniformIndex { 0 };
        size_t elementIndex { 0 };
    };
    struct ProgramRecord {
        std::vector<PlatformGLObject> shaders;
        std::unordered_map<std::string, GCGLuint> requestedLocations;
        std::vector<AttributeRecord> attributes;
        std::vector<UniformRecord> uniforms;
        std::unordered_map<GCGLint, UniformLocationInfo> uniformLocations;
        GX2VertexShader* vertexShader { nullptr };
        GX2PixelShader* pixelShader { nullptr };
        GX2ShaderMode mode { GLSL_SHADER_MODE_ERROR };
        std::string log;
        GCGLint nextUniformLocation { 0 };
        bool linked { false };
        bool validated { false };
        bool deletePending { false };
    };
    PlatformGLObject nextShaderName();
    PlatformGLObject nextProgramName();
    PlatformGLObject nextTextureName();
    void releaseTextureStorage(TextureRecord&);
    bool recreateTextureStorage(TextureRecord&);
    bool uploadTextureLevel(TextureRecord&, uint32_t level);
    bool convertTexturePixels(GCGLenum format, GCGLenum type, GCGLsizei width, GCGLsizei height,
        std::span<const uint8_t> pixels, std::vector<uint8_t>& output) const;
    bool regenerateTextureMipmaps(TextureRecord&);
    void applyTextureSampler(TextureRecord&);
    bool applyProgramSamplers(ProgramRecord&);
    TextureRecord* boundTexture2D();
    void releaseShaderIfUnused(PlatformGLObject);
    void addError(GCGLenum);
    void addDriverError(WKC::GLGX2::Result, const char*);
    void clearActiveNativeProgram();
    void releaseProgramNative(ProgramRecord&);
    bool activateProgram(PlatformGLObject);
    void buildProgramInterface(ProgramRecord&);
    bool setUniformData(GCGLint, const void*, size_t, uint32_t components, uint32_t count, bool integerValues, uint32_t matrixDimension = 0);
    bool uploadUniformElement(const UniformBinding&, GCGLenum, const void*, bool integerValues);
    static WKC::GLGX2::BufferUsage mapBufferUsage(GCGLenum);
    WKC::GLGX2::WUTBackendContext m_native;
    WKC::GLGX2::Backend m_stateBackend;
    WKC::GLGX2::Driver m_driver;
    WKC::GLGX2::BufferBackend m_bufferBackend;
    WKC::GLGX2::BufferObjects m_buffers;
    WKC::GLGX2::VertexArrayBackend m_vertexArrayBackend;
    WKC::GLGX2::VertexArrays m_vertexArrays;
    GCGLErrorCodeSet m_errors;
    std::unordered_map<PlatformGLObject, ShaderRecord> m_shaders;
    std::unordered_map<PlatformGLObject, ProgramRecord> m_programs;
    std::unordered_map<PlatformGLObject, TextureRecord> m_textures;
    std::unordered_map<PlatformGLObject, bool> m_vertexArrayObjects;
    TextureRecord m_defaultTexture;
    std::array<PlatformGLObject, 16> m_boundTextures2D {};
    PlatformGLObject m_nextShader { 1 };
    PlatformGLObject m_nextProgram { 1 };
    PlatformGLObject m_nextTexture { 1 };
    PlatformGLObject m_currentProgram { 0 };
    PlatformGLObject m_appliedProgram { 0 };
    PlatformGLObject m_boundFramebuffer { 0 };
    GCGLuint m_activeTextureUnit { 0 };
    GCGLint m_unpackAlignment { 4 };
    GCGLint m_packAlignment { 4 };
    bool m_defaultTextureReady { false };
    bool m_compilerAvailable { false };
};

};


GraphicsContextGLWKC::GraphicsContextGLWKC(const GraphicsContextGLAttributes& attrs)
    : GraphicsContextGL(attrs)
    , m_stateBackend(WKC::GLGX2::makeWUTBackend(m_native))
    , m_driver(m_stateBackend)
    , m_bufferBackend(WKC::GLGX2::makeWUTBufferBackend(m_native))
    , m_buffers(m_bufferBackend)
    , m_vertexArrayBackend(WKC::GLGX2::makeWUTVertexArrayBackend(m_native))
    , m_vertexArrays(m_vertexArrayBackend)
{
    m_vertexArrayObjects.emplace(0, true);
    initializeGLSLCompilerOnce();
    m_compilerAvailable = GetGLSLCompilerVersion() != nullptr;
}

GraphicsContextGLWKC::~GraphicsContextGLWKC()
{
    GX2DrawDone();
    clearActiveNativeProgram();
    for (auto& entry : m_shaders) {
        FreeVertexShader(entry.second.compiledVertex);
        FreePixelShader(entry.second.compiledPixel);
    }
    for (auto& entry : m_programs)
        releaseProgramNative(entry.second);
    for (auto& entry : m_textures)
        releaseTextureStorage(entry.second);
    releaseTextureStorage(m_defaultTexture);
}

PlatformGLObject GraphicsContextGLWKC::nextTextureName()
{
    for (uint64_t attempts = 0; attempts < UINT32_MAX - 1ULL; ++attempts) {
        PlatformGLObject name = m_nextTexture++;
        if (!m_nextTexture)
            m_nextTexture = 1;
        if (name && !m_textures.contains(name))
            return name;
    }
    return 0;
}

void GraphicsContextGLWKC::releaseTextureStorage(TextureRecord& texture)
{
    if (!texture.storageCreated)
        return;
    GX2DrawDone();
    GX2RDestroySurfaceEx(&texture.native.surface, textureSurfaceFlags);
    texture.native = {};
    texture.storageCreated = false;
}

bool GraphicsContextGLWKC::uploadTextureLevel(TextureRecord& texture, uint32_t level)
{
    if (!texture.storageCreated || level >= texture.levels.size())
        return false;

    const uint32_t width = std::max<uint32_t>(1, texture.width >> level);
    const uint32_t height = std::max<uint32_t>(1, texture.height >> level);
    const auto& source = texture.levels[level];
    if (source.size() != static_cast<size_t>(width) * height * 4)
        return false;

    void* locked = GX2RLockSurfaceEx(&texture.native.surface, static_cast<int32_t>(level),
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
    if (!locked)
        return false;

    // Linear-aligned RGBA8 surfaces expose rows with an aligned texel pitch.
    // GX2RLockSurfaceEx selects the requested mip level, so level-local rows
    // can be filled without overwriting adjacent mip levels.
    const uint32_t pitchPixels = std::max<uint32_t>(width, texture.native.surface.pitch >> level);
    if (pitchPixels < width) {
        GX2RUnlockSurfaceEx(&texture.native.surface, static_cast<int32_t>(level),
            static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
        return false;
    }
    const size_t destinationPitch = static_cast<size_t>(pitchPixels) * 4;
    for (uint32_t y = 0; y < height; ++y) {
        std::memcpy(static_cast<uint8_t*>(locked) + static_cast<size_t>(y) * destinationPitch,
            source.data() + static_cast<size_t>(y) * width * 4,
            static_cast<size_t>(width) * 4);
    }
    GX2RUnlockSurfaceEx(&texture.native.surface, static_cast<int32_t>(level),
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
    return true;
}

bool GraphicsContextGLWKC::recreateTextureStorage(TextureRecord& texture)
{
    if (!texture.width || !texture.height || texture.levels.empty()) {
        releaseTextureStorage(texture);
        return true;
    }
    if (texture.levels.size() > 13 || texture.width > static_cast<uint32_t>(maxTextureSize())
        || texture.height > static_cast<uint32_t>(maxTextureSize()))
        return false;
    uint32_t maximumLevelCount = 1;
    for (uint32_t dimension = std::max(texture.width, texture.height); dimension > 1; dimension >>= 1)
        ++maximumLevelCount;
    if (texture.levels.size() > maximumLevelCount)
        return false;

    releaseTextureStorage(texture);
    texture.native = {};
    auto& surface = texture.native.surface;
    surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    surface.width = texture.width;
    surface.height = texture.height;
    surface.depth = 1;
    surface.mipLevels = static_cast<uint32_t>(texture.levels.size());
    surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    surface.aa = GX2_AA_MODE1X;
    surface.use = GX2_SURFACE_USE_TEXTURE;
    surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    GX2CalcSurfaceSizeAndAlignment(&surface);
    if (!surface.imageSize || !surface.alignment
        || !GX2RCreateSurface(&surface, textureSurfaceFlags)) {
        texture.native = {};
        texture.storageCreated = false;
        return false;
    }

    texture.native.viewFirstMip = 0;
    texture.native.viewNumMips = surface.mipLevels;
    texture.native.viewFirstSlice = 0;
    texture.native.viewNumSlices = 1;
    texture.native.compMap = GX2_COMP_MAP(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W);
    texture.storageCreated = true;

    for (uint32_t level = 0; level < texture.levels.size(); ++level) {
        if (!uploadTextureLevel(texture, level)) {
            releaseTextureStorage(texture);
            return false;
        }
    }
    GX2InitTextureRegs(&texture.native);
    applyTextureSampler(texture);
    return true;
}

bool GraphicsContextGLWKC::convertTexturePixels(GCGLenum format, GCGLenum type,
    GCGLsizei width, GCGLsizei height, std::span<const uint8_t> pixels,
    std::vector<uint8_t>& output) const
{
    if (width < 0 || height < 0 || type != GL_UNSIGNED_BYTE_VALUE)
        return false;

    uint32_t sourceComponents = 0;
    switch (format) {
    case GL_ALPHA_VALUE:
    case GL_LUMINANCE_VALUE:
        sourceComponents = 1;
        break;
    case GL_LUMINANCE_ALPHA_VALUE:
        sourceComponents = 2;
        break;
    case GL_RGB_VALUE:
        sourceComponents = 3;
        break;
    case GL_RGBA_VALUE:
        sourceComponents = 4;
        break;
    default:
        return false;
    }

    const size_t w = static_cast<size_t>(width);
    const size_t h = static_cast<size_t>(height);
    if (w && h > std::numeric_limits<size_t>::max() / w / 4)
        return false;
    output.assign(w * h * 4, 0);
    if (!width || !height || pixels.empty())
        return true;

    const size_t sourceRowBytes = w * sourceComponents;
    const size_t alignment = static_cast<size_t>(m_unpackAlignment);
    const size_t sourceStride = (sourceRowBytes + alignment - 1) & ~(alignment - 1);
    const size_t requiredBytes = (h - 1) * sourceStride + sourceRowBytes;
    if (pixels.size() < requiredBytes)
        return false;

    for (size_t y = 0; y < h; ++y) {
        const uint8_t* row = pixels.data() + y * sourceStride;
        for (size_t x = 0; x < w; ++x) {
            const uint8_t* source = row + x * sourceComponents;
            uint8_t* destination = output.data() + (y * w + x) * 4;
            switch (format) {
            case GL_ALPHA_VALUE:
                destination[0] = destination[1] = destination[2] = 255;
                destination[3] = source[0];
                break;
            case GL_LUMINANCE_VALUE:
                destination[0] = destination[1] = destination[2] = source[0];
                destination[3] = 255;
                break;
            case GL_LUMINANCE_ALPHA_VALUE:
                destination[0] = destination[1] = destination[2] = source[0];
                destination[3] = source[1];
                break;
            case GL_RGB_VALUE:
                destination[0] = source[0];
                destination[1] = source[1];
                destination[2] = source[2];
                destination[3] = 255;
                break;
            case GL_RGBA_VALUE:
                std::memcpy(destination, source, 4);
                break;
            default:
                return false;
            }
        }
    }
    return true;
}

void GraphicsContextGLWKC::applyTextureSampler(TextureRecord& texture)
{
    auto translateWrap = [](GCGLint wrap, GX2TexClampMode& output) {
        switch (wrap) {
        case GL_REPEAT_VALUE: output = GX2_TEX_CLAMP_MODE_WRAP; return true;
        case GL_CLAMP_TO_EDGE_VALUE: output = GX2_TEX_CLAMP_MODE_CLAMP; return true;
        case GL_MIRRORED_REPEAT_VALUE: output = GX2_TEX_CLAMP_MODE_MIRROR; return true;
        default: return false;
        }
    };
    auto filterMode = [](GCGLint filter) {
        return filter == GL_NEAREST_VALUE || filter == GL_NEAREST_MIPMAP_NEAREST_VALUE
            || filter == GL_NEAREST_MIPMAP_LINEAR_VALUE
            ? GX2_TEX_XY_FILTER_MODE_POINT : GX2_TEX_XY_FILTER_MODE_LINEAR;
    };
    auto mipMode = [](GCGLint filter) {
        switch (filter) {
        case GL_NEAREST_MIPMAP_NEAREST_VALUE:
        case GL_LINEAR_MIPMAP_NEAREST_VALUE:
            return GX2_TEX_MIP_FILTER_MODE_POINT;
        case GL_NEAREST_MIPMAP_LINEAR_VALUE:
        case GL_LINEAR_MIPMAP_LINEAR_VALUE:
            return GX2_TEX_MIP_FILTER_MODE_LINEAR;
        default:
            return GX2_TEX_MIP_FILTER_MODE_NONE;
        }
    };

    GX2TexClampMode clampS, clampT;
    if (!translateWrap(texture.wrapS, clampS) || !translateWrap(texture.wrapT, clampT))
        return;
    const auto minFilter = filterMode(texture.minFilter);
    const auto magFilter = filterMode(texture.magFilter);
    GX2InitSampler(&texture.sampler, clampS, minFilter);
    GX2InitSamplerClamping(&texture.sampler, clampS, clampT, clampS);
    GX2InitSamplerXYFilter(&texture.sampler, magFilter, minFilter, GX2_TEX_ANISO_RATIO_NONE);
    GX2InitSamplerZMFilter(&texture.sampler, GX2_TEX_Z_FILTER_MODE_NONE, mipMode(texture.minFilter));
    GX2InitSamplerLOD(&texture.sampler, 0.0f, 1000.0f, 0.0f);
}

GraphicsContextGLWKC::TextureRecord* GraphicsContextGLWKC::boundTexture2D()
{
    if (m_activeTextureUnit >= m_boundTextures2D.size())
        return nullptr;
    auto name = m_boundTextures2D[m_activeTextureUnit];
    auto it = m_textures.find(name);
    if (!name || it == m_textures.end() || it->second.target != GL_TEXTURE_2D_VALUE)
        return nullptr;
    return &it->second;
}

bool GraphicsContextGLWKC::regenerateTextureMipmaps(TextureRecord& texture)
{
    if (!texture.width || !texture.height || texture.levels.empty() || !texture.definedLevels[0])
        return false;

    const auto base = texture.levels[0];
    texture.levels.clear();
    texture.definedLevels.clear();
    texture.levels.push_back(base);
    texture.definedLevels.push_back(true);

    uint32_t sourceWidth = texture.width;
    uint32_t sourceHeight = texture.height;
    while (sourceWidth > 1 || sourceHeight > 1) {
        const uint32_t destinationWidth = std::max<uint32_t>(1, sourceWidth / 2);
        const uint32_t destinationHeight = std::max<uint32_t>(1, sourceHeight / 2);
        std::vector<uint8_t> destination(static_cast<size_t>(destinationWidth) * destinationHeight * 4, 0);
        const auto& source = texture.levels.back();
        for (uint32_t y = 0; y < destinationHeight; ++y) {
            for (uint32_t x = 0; x < destinationWidth; ++x) {
                uint32_t sums[4] { 0, 0, 0, 0 };
                uint32_t sampleCount = 0;
                for (uint32_t dy = 0; dy < 2; ++dy) {
                    const uint32_t sy = std::min(sourceHeight - 1, y * 2 + dy);
                    for (uint32_t dx = 0; dx < 2; ++dx) {
                        const uint32_t sx = std::min(sourceWidth - 1, x * 2 + dx);
                        const size_t offset = (static_cast<size_t>(sy) * sourceWidth + sx) * 4;
                        for (unsigned component = 0; component < 4; ++component)
                            sums[component] += source[offset + component];
                        ++sampleCount;
                    }
                }
                const size_t outputOffset = (static_cast<size_t>(y) * destinationWidth + x) * 4;
                for (unsigned component = 0; component < 4; ++component)
                    destination[outputOffset + component] = static_cast<uint8_t>(sums[component] / sampleCount);
            }
        }
        texture.levels.push_back(std::move(destination));
        texture.definedLevels.push_back(true);
        sourceWidth = destinationWidth;
        sourceHeight = destinationHeight;
    }
    return recreateTextureStorage(texture);
}

bool GraphicsContextGLWKC::applyProgramSamplers(ProgramRecord& program)
{
    if (!m_defaultTextureReady)
        return false;

    for (const auto& uniform : program.uniforms) {
        if (uniform.type != GL_SAMPLER_2D_VALUE && uniform.type != GL_SAMPLER_CUBE_VALUE)
            continue;
        for (size_t index = 0; index < uniform.elements.size(); ++index) {
            const auto& binding = uniform.elements[index];
            const GCGLint unit = index < uniform.samplerUnits.size() ? uniform.samplerUnits[index] : 0;
            if (unit < 0 || unit >= static_cast<GCGLint>(m_boundTextures2D.size()))
                return false;

            TextureRecord* texture = nullptr;
            auto name = m_boundTextures2D[unit];
            auto found = m_textures.find(name);
            if (uniform.type == GL_SAMPLER_2D_VALUE && name && found != m_textures.end() && found->second.storageCreated
                && !found->second.levels.empty() && !found->second.definedLevels.empty()
                && found->second.definedLevels[0] && found->second.target == GL_TEXTURE_2D_VALUE) {
                const bool needsMipmaps = uniform.type == GL_SAMPLER_2D_VALUE
                    && (found->second.minFilter == GL_NEAREST_MIPMAP_NEAREST_VALUE
                        || found->second.minFilter == GL_LINEAR_MIPMAP_NEAREST_VALUE
                        || found->second.minFilter == GL_NEAREST_MIPMAP_LINEAR_VALUE
                        || found->second.minFilter == GL_LINEAR_MIPMAP_LINEAR_VALUE);
                bool complete = true;
                if (needsMipmaps) {
                    for (bool defined : found->second.definedLevels)
                        complete &= defined;
                    uint32_t expectedLevels = 1;
                    for (uint32_t dimension = std::max(found->second.width, found->second.height);
                         dimension > 1; dimension >>= 1)
                        ++expectedLevels;
                    complete &= found->second.levels.size() == expectedLevels;
                }
                if (complete)
                    texture = &found->second;
            }
            if (!texture)
                texture = &m_defaultTexture;

            applyTextureSampler(*texture);
            if (binding.vertexSamplerVar) {
                GX2SetVertexTexture(&texture->native, binding.vertexSamplerVar->location);
                GX2SetVertexSampler(&texture->sampler, binding.vertexSamplerVar->location);
            }
            if (binding.pixelSamplerVar) {
                GX2SetPixelTexture(&texture->native, binding.pixelSamplerVar->location);
                GX2SetPixelSampler(&texture->sampler, binding.pixelSamplerVar->location);
            }
        }
    }
    return true;
}

bool GraphicsContextGLWKC::initialize()
{
    if (!m_compilerAvailable || !WKC::GLGX2::resizeWUTRenderTarget(m_native, 1, 1))
        return false;
    m_currentWidth = m_currentHeight = 1;
    addDriverError(m_driver.setViewport({ 0, 0, 1, 1, 0.0f, 1.0f }), "initial viewport");

    m_defaultTexture.target = GL_TEXTURE_2D_VALUE;
    m_defaultTexture.width = 1;
    m_defaultTexture.height = 1;
    m_defaultTexture.levels = { std::vector<uint8_t> { 0, 0, 0, 255 } };
    m_defaultTexture.definedLevels = { true };
    m_defaultTextureReady = recreateTextureStorage(m_defaultTexture);
    return m_defaultTextureReady;
}

PlatformGLObject GraphicsContextGLWKC::nextShaderName()
{
    for (uint64_t i=0; i<UINT32_MAX-1ULL; ++i) {
        PlatformGLObject name=m_nextShader++;
        if (!m_nextShader) m_nextShader=1;
        if (name && !m_shaders.contains(name)) return name;
    }
    return 0;
}
PlatformGLObject GraphicsContextGLWKC::nextProgramName()
{
    for (uint64_t i=0; i<UINT32_MAX-1ULL; ++i) {
        PlatformGLObject name=m_nextProgram++;
        if (!m_nextProgram) m_nextProgram=1;
        if (name && !m_programs.contains(name)) return name;
    }
    return 0;
}
void GraphicsContextGLWKC::addError(GCGLenum error)
{
    m_errors.add(GraphicsContextGL::enumToErrorCode(error));
}
void GraphicsContextGLWKC::addDriverError(WKC::GLGX2::Result result, const char* operation)
{
    if (result == WKC::GLGX2::Result::Ok) return;
    addError(result == WKC::GLGX2::Result::InvalidArgument ? GL_INVALID_VALUE_VALUE : GL_INVALID_OPERATION_VALUE);
    m_driver.unsupported(operation);
}
WKC::GLGX2::BufferUsage GraphicsContextGLWKC::mapBufferUsage(GCGLenum usage)
{
    switch(usage) {
    case GL_STATIC_DRAW_VALUE: return WKC::GLGX2::BufferUsage::StaticDraw;
    case GL_DYNAMIC_DRAW_VALUE: return WKC::GLGX2::BufferUsage::DynamicDraw;
    case GL_STREAM_DRAW_VALUE: return WKC::GLGX2::BufferUsage::StreamDraw;
    default: return WKC::GLGX2::BufferUsage::StaticDraw;
    }
}
void GraphicsContextGLWKC::clearActiveNativeProgram()
{
    if (!m_native.bufferResources.vertexShader && !m_native.bufferResources.pixelShader) return;
    GX2DrawDone();
    m_native.bufferResources.vertexShader=nullptr;
    m_native.bufferResources.pixelShader=nullptr;
    m_native.bufferResources.linkedAttributes.clear();
    m_native.bufferResources.fetchShaderDirty=true;
    m_appliedProgram=0;
}
void GraphicsContextGLWKC::releaseProgramNative(ProgramRecord& p)
{
    if (!p.vertexShader && !p.pixelShader) return;
    if (m_appliedProgram && m_currentProgram==m_appliedProgram) clearActiveNativeProgram();
    else GX2DrawDone();
    FreeShaders(p.vertexShader,p.pixelShader);
    p.vertexShader=nullptr; p.pixelShader=nullptr;
    p.mode=GLSL_SHADER_MODE_ERROR;
    p.attributes.clear(); p.uniforms.clear(); p.uniformLocations.clear();
    p.linked=false;
}
bool GraphicsContextGLWKC::activateProgram(PlatformGLObject name)
{
    if (!name) { clearActiveNativeProgram(); m_currentProgram=0; return true; }
    auto it=m_programs.find(name);
    if (it==m_programs.end() || !it->second.linked || !it->second.vertexShader || !it->second.pixelShader) {
        addError(GL_INVALID_OPERATION_VALUE); return false;
    }
    if (m_appliedProgram==name && m_native.bufferResources.vertexShader==it->second.vertexShader)
        return applyProgramSamplers(it->second);
    std::vector<WKC::GLGX2::WUTLinkedAttribute> bindings;
    bindings.reserve(it->second.attributes.size());
    for (const auto& a:it->second.attributes)
        bindings.push_back({static_cast<uint8_t>(a.location),a.name.c_str()});
    if (!WKC::GLGX2::setWUTLinkedProgram(m_native.bufferResources,*it->second.vertexShader,*it->second.pixelShader,
            it->second.mode,bindings.data(),bindings.size())) {
        addError(GL_INVALID_OPERATION_VALUE); clearActiveNativeProgram(); return false;
    }
    m_currentProgram=name; m_appliedProgram=name;
    if (!applyProgramSamplers(it->second)) {
        addError(GL_INVALID_OPERATION_VALUE);
        return false;
    }
    return true;
}
void GraphicsContextGLWKC::buildProgramInterface(ProgramRecord& p)
{
    p.attributes.clear(); p.uniforms.clear(); p.uniformLocations.clear(); p.nextUniformLocation=0;
    if (!p.vertexShader || !p.pixelShader) return;
    std::vector<bool> used(32,false);
    for (const auto& wanted:p.requestedLocations) {
        if (wanted.second>=32) { p.log="Attribute location exceeds GX2 vertex-fetch limit."; p.linked=false; return; }
        if (used[wanted.second]) { p.log="Conflicting bound attribute locations."; p.linked=false; return; }
        used[wanted.second]=true;
    }
    for (uint32_t i=0;i<p.vertexShader->attribVarCount;++i) {
        const auto& native=p.vertexShader->attribVars[i];
        if (!native.name) continue;
        AttributeRecord a;
        a.name=native.name; a.type=mapGX2VariableType(native.type); a.size=native.count ? native.count : 1;
        auto requested=p.requestedLocations.find(a.name);
        if (requested!=p.requestedLocations.end()) a.location=requested->second;
        else if (native.location<32 && !used[native.location]) { a.location=native.location; used[a.location]=true; }
        else {
            auto free=std::find(used.begin(),used.end(),false);
            if (free==used.end()) { p.log="GX2 supports at most 32 vertex attributes."; p.linked=false; return; }
            a.location=static_cast<GCGLuint>(free-used.begin()); *free=true;
        }
        if (!a.type) { p.log="GX2 shader reported an unsupported WebGL attribute type."; p.linked=false; return; }
        p.attributes.push_back(std::move(a));
    }
    auto addUniforms=[&](auto* shader,bool vertexStage) {
        if (!shader || !shader->uniformVars) return;
        for(uint32_t i=0;i<shader->uniformVarCount;++i) {
            const auto& native=shader->uniformVars[i];
            if(!native.name) continue;
            std::string name(native.name);
            auto found=std::find_if(p.uniforms.begin(),p.uniforms.end(),[&](const UniformRecord& u){return u.name==name;});
            if(found==p.uniforms.end()) {
                UniformRecord u; u.name=name; u.type=mapGX2VariableType(native.type);
                u.size=static_cast<GCGLint>(native.count?native.count:1);
                if(!u.type) continue;
                u.elements.resize(u.size);
                p.uniforms.push_back(std::move(u)); found=std::prev(p.uniforms.end());
            }
            const size_t count=std::min<size_t>(found->elements.size(),native.count?native.count:1);
            for(size_t el=0;el<count;++el) {
                auto& b=found->elements[el];
                if(vertexStage) b.vertexVar=&native; else b.pixelVar=&native;
                b.arrayElement=static_cast<uint32_t>(el);
            }
        }
    };
    addUniforms(p.vertexShader,true); addUniforms(p.pixelShader,false);
    auto addSamplers = [&](auto* shader, bool vertexStage) {
        if (!shader || !shader->samplerVars)
            return;
        for (uint32_t i = 0; i < shader->samplerVarCount; ++i) {
            const auto& native = shader->samplerVars[i];
            if (!native.name)
                continue;
            GCGLenum glType = 0;
            if (native.type == GX2_SAMPLER_VAR_TYPE_SAMPLER_2D)
                glType = GL_SAMPLER_2D_VALUE;
            else if (native.type == GX2_SAMPLER_VAR_TYPE_SAMPLER_CUBE)
                glType = GL_SAMPLER_CUBE_VALUE;
            else
                continue;

            std::string uniformName(native.name);
            auto found = std::find_if(p.uniforms.begin(), p.uniforms.end(),
                [&](const UniformRecord& uniform) { return uniform.name == uniformName; });
            if (found == p.uniforms.end()) {
                UniformRecord uniform;
                uniform.name = uniformName;
                uniform.type = glType;
                uniform.size = 1;
                uniform.elements.resize(1);
                uniform.samplerUnits = { 0 };
                p.uniforms.push_back(std::move(uniform));
                found = std::prev(p.uniforms.end());
            }
            if (found->type != glType)
                continue;
            if (found->elements.empty())
                found->elements.resize(1);
            if (found->samplerUnits.empty())
                found->samplerUnits.resize(found->elements.size(), 0);
            if (vertexStage)
                found->elements[0].vertexSamplerVar = &native;
            else
                found->elements[0].pixelSamplerVar = &native;
        }
    };
    addSamplers(p.vertexShader, true);
    addSamplers(p.pixelShader, false);
    for(size_t i=0;i<p.uniforms.size();++i)
        for(size_t el=0;el<p.uniforms[i].elements.size();++el) {
            auto& b=p.uniforms[i].elements[el]; b.location=p.nextUniformLocation++;
            p.uniformLocations.emplace(b.location,UniformLocationInfo{i,el});
        }
}
bool GraphicsContextGLWKC::uploadUniformElement(const UniformBinding& b,GCGLenum type,const void* values,bool ints)
{
    if (b.vertexSamplerVar || b.pixelSamplerVar)
        return values && ints;
    if(!values) return false;
    std::array<uint32_t,16> packed{};
    const auto components=uniformComponents(type), regs=uniformRegisters(type);
    if(regs>1) {
        const auto* f=static_cast<const float*>(values);
        const uint32_t dim=regs;
        for(uint32_t col=0;col<dim;++col) for(uint32_t row=0;row<dim;++row)
            std::memcpy(&packed[col*4+row],&f[col*dim+row],sizeof(float));
    } else {
        const uint32_t n=std::min<uint32_t>(components,4);
        if(ints) {
            const auto* in=static_cast<const int32_t*>(values);
            for(uint32_t i=0;i<n;++i) packed[i]=static_cast<uint32_t>(in[i]);
        } else std::memcpy(packed.data(),values,n*sizeof(float));
    }
    if(b.vertexVar) GX2SetVertexUniformReg(b.vertexVar->offset+b.arrayElement*regs,regs,packed.data());
    if(b.pixelVar) GX2SetPixelUniformReg(b.pixelVar->offset+b.arrayElement*regs,regs,packed.data());
    return true;
}
bool GraphicsContextGLWKC::setUniformData(GCGLint loc,const void* values,size_t bytes,uint32_t components,uint32_t count,bool ints,uint32_t matrixDim)
{
    if(loc==-1) return true;
    if(!m_currentProgram || !values) { addError(GL_INVALID_OPERATION_VALUE); return false; }
    auto p=m_programs.find(m_currentProgram);
    if(p==m_programs.end() || !p->second.linked) { addError(GL_INVALID_OPERATION_VALUE); return false; }
    auto l=p->second.uniformLocations.find(loc);
    if(l==p->second.uniformLocations.end()) { addError(GL_INVALID_OPERATION_VALUE); return false; }
    auto& uniform=p->second.uniforms[l->second.uniformIndex];
    const bool matrixType = uniform.type == GL_FLOAT_MAT2_VALUE
        || uniform.type == GL_FLOAT_MAT3_VALUE || uniform.type == GL_FLOAT_MAT4_VALUE;
    const bool integerType = uniform.type == GL_INT_VALUE || uniform.type == GL_BOOL_VALUE
        || uniform.type == GL_INT_VEC2_VALUE || uniform.type == GL_INT_VEC3_VALUE || uniform.type == GL_INT_VEC4_VALUE
        || uniform.type == GL_BOOL_VEC2_VALUE || uniform.type == GL_BOOL_VEC3_VALUE || uniform.type == GL_BOOL_VEC4_VALUE
        || uniform.type == 0x8B5E /* SAMPLER_2D */ || uniform.type == 0x8B60 /* SAMPLER_CUBE */;
    if (matrixType != !!matrixDim || (matrixDim && (matrixDim != uniformRegisters(uniform.type)
            || components != matrixDim * matrixDim))
        || (!matrixDim && (uniformComponents(uniform.type) != components || ints != integerType))) {
        addError(GL_INVALID_OPERATION_VALUE);
        return false;
    }
    if (l->second.elementIndex > uniform.elements.size()
        || count > uniform.elements.size() - l->second.elementIndex) {
        addError(GL_INVALID_OPERATION_VALUE);
        return false;
    }
    if (bytes < static_cast<size_t>(components) * count * (ints ? sizeof(int32_t) : sizeof(float))) {
        addError(GL_INVALID_VALUE_VALUE);
        return false;
    }
    const auto* data = static_cast<const uint8_t*>(values);
    const bool samplerUniform = uniform.type == GL_SAMPLER_2D_VALUE || uniform.type == GL_SAMPLER_CUBE_VALUE;
    const size_t scalarBytes = ints ? sizeof(int32_t) : sizeof(float);
    for (uint32_t i = 0; i < count; ++i) {
        const size_t element = l->second.elementIndex + i;
        const void* elementData = data + static_cast<size_t>(i) * components * scalarBytes;
        if (samplerUniform) {
            const int32_t unit = *static_cast<const int32_t*>(elementData);
            if (unit < 0 || unit >= static_cast<int32_t>(m_boundTextures2D.size())) {
                addError(GL_INVALID_VALUE_VALUE);
                return false;
            }
            if (element >= uniform.samplerUnits.size())
                uniform.samplerUnits.resize(uniform.elements.size(), 0);
            uniform.samplerUnits[element] = unit;
            uniform.elements[element].value.assign(data + static_cast<size_t>(i) * components * scalarBytes,
                data + static_cast<size_t>(i + 1) * components * scalarBytes);
            uniform.elements[element].valueSet = true;
            continue;
        }
        if (!uploadUniformElement(uniform.elements[element], uniform.type, elementData, ints)) {
            addError(GL_INVALID_OPERATION_VALUE);
            return false;
        }
        uniform.elements[element].value.assign(data + static_cast<size_t>(i) * components * scalarBytes,
            data + static_cast<size_t>(i + 1) * components * scalarBytes);
        uniform.elements[element].valueSet = true;
    }
    if (samplerUniform && !applyProgramSamplers(p->second)) {
        addError(GL_INVALID_OPERATION_VALUE);
        return false;
    }
    return true;
}
RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLWKC::layerContentsDisplayDelegate() { return nullptr; }

void GraphicsContextGLWKC::activeTexture(GCGLenum tex)
{
    if(tex < GL_TEXTURE0_VALUE || tex >= GL_TEXTURE0_VALUE + m_boundTextures2D.size()) {
        addError(GL_INVALID_ENUM_VALUE);
        return;
    }
    m_activeTextureUnit = tex - GL_TEXTURE0_VALUE;
}

PlatformGLObject GraphicsContextGLWKC::createTexture()
{
    const auto name = nextTextureName();
    if (!name) {
        addError(GL_OUT_OF_MEMORY_VALUE);
        return 0;
    }
    m_textures.emplace(name, TextureRecord {});
    return name;
}

void GraphicsContextGLWKC::deleteTexture(PlatformGLObject name)
{
    auto it = m_textures.find(name);
    if (!name || it == m_textures.end())
        return;
    releaseTextureStorage(it->second);
    m_textures.erase(it);
    for (auto& bound : m_boundTextures2D) {
        if (bound == name)
            bound = 0;
    }
}

GCGLboolean GraphicsContextGLWKC::isTexture(PlatformGLObject name)
{
    auto it = m_textures.find(name);
    return name && it != m_textures.end() && it->second.target != 0;
}

void GraphicsContextGLWKC::bindTexture(GCGLenum target, PlatformGLObject name)
{
    if (target != GL_TEXTURE_2D_VALUE) {
        addError(GL_INVALID_ENUM_VALUE);
        return;
    }
    if (m_activeTextureUnit >= m_boundTextures2D.size()) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    if (!name) {
        m_boundTextures2D[m_activeTextureUnit] = 0;
        return;
    }
    auto it = m_textures.find(name);
    if (it == m_textures.end()) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    if (it->second.target && it->second.target != target) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    it->second.target = target;
    m_boundTextures2D[m_activeTextureUnit] = name;
    applyTextureSampler(it->second);
}

GCGLint GraphicsContextGLWKC::getTexParameteri(GCGLenum target, GCGLenum pname)
{
    if (target != GL_TEXTURE_2D_VALUE) {
        addError(GL_INVALID_ENUM_VALUE);
        return 0;
    }
    auto* texture = boundTexture2D();
    if (!texture) {
        addError(GL_INVALID_OPERATION_VALUE);
        return 0;
    }
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER_VALUE: return texture->minFilter;
    case GL_TEXTURE_MAG_FILTER_VALUE: return texture->magFilter;
    case GL_TEXTURE_WRAP_S_VALUE: return texture->wrapS;
    case GL_TEXTURE_WRAP_T_VALUE: return texture->wrapT;
    default:
        addError(GL_INVALID_ENUM_VALUE);
        return 0;
    }
}

GCGLfloat GraphicsContextGLWKC::getTexParameterf(GCGLenum target, GCGLenum pname)
{
    return static_cast<GCGLfloat>(getTexParameteri(target, pname));
}

void GraphicsContextGLWKC::texParameteri(GCGLenum target, GCGLenum pname, GCGLint param)
{
    if (target != GL_TEXTURE_2D_VALUE) {
        addError(GL_INVALID_ENUM_VALUE);
        return;
    }
    auto* texture = boundTexture2D();
    if (!texture) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }

    switch (pname) {
    case GL_TEXTURE_MIN_FILTER_VALUE:
        if (param != GL_NEAREST_VALUE && param != GL_LINEAR_VALUE
            && param != GL_NEAREST_MIPMAP_NEAREST_VALUE && param != GL_LINEAR_MIPMAP_NEAREST_VALUE
            && param != GL_NEAREST_MIPMAP_LINEAR_VALUE && param != GL_LINEAR_MIPMAP_LINEAR_VALUE) {
            addError(GL_INVALID_ENUM_VALUE);
            return;
        }
        texture->minFilter = param;
        break;
    case GL_TEXTURE_MAG_FILTER_VALUE:
        if (param != GL_NEAREST_VALUE && param != GL_LINEAR_VALUE) {
            addError(GL_INVALID_ENUM_VALUE);
            return;
        }
        texture->magFilter = param;
        break;
    case GL_TEXTURE_WRAP_S_VALUE:
    case GL_TEXTURE_WRAP_T_VALUE:
        if (param != GL_REPEAT_VALUE && param != GL_CLAMP_TO_EDGE_VALUE
            && param != GL_MIRRORED_REPEAT_VALUE) {
            addError(GL_INVALID_ENUM_VALUE);
            return;
        }
        if (pname == GL_TEXTURE_WRAP_S_VALUE)
            texture->wrapS = param;
        else
            texture->wrapT = param;
        break;
    default:
        addError(GL_INVALID_ENUM_VALUE);
        return;
    }
    applyTextureSampler(*texture);
}

void GraphicsContextGLWKC::texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat param)
{
    if (!std::isfinite(param) || param < 0 || param > static_cast<GCGLfloat>(INT32_MAX)) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }
    const auto integral = static_cast<GCGLint>(param);
    if (static_cast<GCGLfloat>(integral) != param) {
        addError(GL_INVALID_ENUM_VALUE);
        return;
    }
    texParameteri(target, pname, integral);
}

void GraphicsContextGLWKC::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (pname != GL_UNPACK_ALIGNMENT_VALUE && pname != GL_PACK_ALIGNMENT_VALUE) {
        addError(GL_INVALID_ENUM_VALUE);
        return;
    }
    if (param != 1 && param != 2 && param != 4 && param != 8) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }
    if (pname == GL_UNPACK_ALIGNMENT_VALUE)
        m_unpackAlignment = param;
    else
        m_packAlignment = param;
}

void GraphicsContextGLWKC::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat,
    GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type,
    std::span<const uint8_t> pixels)
{
    if (target != GL_TEXTURE_2D_VALUE) {
        addError(GL_INVALID_ENUM_VALUE);
        return;
    }
    if (level < 0 || level >= 13 || width < 0 || height < 0
        || width > maxTextureSize() || height > maxTextureSize() || border) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }
    if (internalformat != format) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    auto* texture = boundTexture2D();
    if (!texture) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    if (!width || !height) {
        if (level != 0) {
            addError(GL_INVALID_VALUE_VALUE);
            return;
        }
        releaseTextureStorage(*texture);
        texture->width = texture->height = 0;
        texture->levels.clear();
        texture->definedLevels.clear();
        return;
    }
    if (level && (!texture->width || !texture->height)) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    const uint32_t baseWidth = level ? texture->width : static_cast<uint32_t>(width);
    const uint32_t baseHeight = level ? texture->height : static_cast<uint32_t>(height);
    uint32_t maxMipLevels = 1;
    for (uint32_t dimension = std::max(baseWidth, baseHeight); dimension > 1; dimension >>= 1)
        ++maxMipLevels;
    if (static_cast<uint32_t>(level) >= maxMipLevels) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }
    const uint32_t expectedWidth = level ? std::max<uint32_t>(1, texture->width >> level) : static_cast<uint32_t>(width);
    const uint32_t expectedHeight = level ? std::max<uint32_t>(1, texture->height >> level) : static_cast<uint32_t>(height);
    if (static_cast<uint32_t>(width) != expectedWidth || static_cast<uint32_t>(height) != expectedHeight) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }

    std::vector<uint8_t> converted;
    if (!convertTexturePixels(format, type, width, height, pixels, converted)) {
        addError(type != GL_UNSIGNED_BYTE_VALUE ? GL_INVALID_ENUM_VALUE : GL_INVALID_VALUE_VALUE);
        return;
    }
    if (!level) {
        releaseTextureStorage(*texture);
        texture->width = static_cast<uint32_t>(width);
        texture->height = static_cast<uint32_t>(height);
        texture->levels.clear();
        texture->definedLevels.clear();
    }
    const size_t levelCount = static_cast<size_t>(level) + 1;
    if (texture->levels.size() < levelCount) {
        texture->levels.resize(levelCount);
        texture->definedLevels.resize(levelCount, false);
    }
    for (size_t mip = 0; mip < texture->levels.size(); ++mip) {
        const uint32_t mipWidth = std::max<uint32_t>(1, texture->width >> mip);
        const uint32_t mipHeight = std::max<uint32_t>(1, texture->height >> mip);
        const size_t expectedBytes = static_cast<size_t>(mipWidth) * mipHeight * 4;
        if (texture->levels[mip].size() != expectedBytes)
            texture->levels[mip].assign(expectedBytes, 0);
    }
    texture->levels[level] = std::move(converted);
    texture->definedLevels[level] = true;
    if (!recreateTextureStorage(*texture))
        addError(GL_OUT_OF_MEMORY_VALUE);
}

void GraphicsContextGLWKC::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset,
    GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type,
    std::span<const uint8_t> pixels)
{
    if (target != GL_TEXTURE_2D_VALUE) {
        addError(GL_INVALID_ENUM_VALUE);
        return;
    }
    if (level < 0 || level >= 13 || xoffset < 0 || yoffset < 0 || width < 0 || height < 0) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }
    if (!width || !height)
        return;
    auto* texture = boundTexture2D();
    if (!texture || !texture->storageCreated || static_cast<size_t>(level) >= texture->levels.size()
        || !texture->definedLevels[level]) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    const uint32_t levelWidth = std::max<uint32_t>(1, texture->width >> level);
    const uint32_t levelHeight = std::max<uint32_t>(1, texture->height >> level);
    if (static_cast<uint32_t>(xoffset) > levelWidth || static_cast<uint32_t>(yoffset) > levelHeight
        || static_cast<uint32_t>(width) > levelWidth - static_cast<uint32_t>(xoffset)
        || static_cast<uint32_t>(height) > levelHeight - static_cast<uint32_t>(yoffset)) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }

    std::vector<uint8_t> converted;
    if (!convertTexturePixels(format, type, width, height, pixels, converted)) {
        addError(type != GL_UNSIGNED_BYTE_VALUE ? GL_INVALID_ENUM_VALUE : GL_INVALID_VALUE_VALUE);
        return;
    }
    auto& destination = texture->levels[level];
    for (GCGLsizei y = 0; y < height; ++y) {
        const size_t destinationOffset =
            (static_cast<size_t>(yoffset + y) * levelWidth + static_cast<size_t>(xoffset)) * 4;
        const size_t sourceOffset = static_cast<size_t>(y) * static_cast<size_t>(width) * 4;
        std::memcpy(destination.data() + destinationOffset, converted.data() + sourceOffset,
            static_cast<size_t>(width) * 4);
    }
    if (!uploadTextureLevel(*texture, static_cast<uint32_t>(level)))
        addError(GL_INVALID_OPERATION_VALUE);
}

void GraphicsContextGLWKC::generateMipmap(GCGLenum target)
{
    if (target != GL_TEXTURE_2D_VALUE) {
        addError(GL_INVALID_ENUM_VALUE);
        return;
    }
    auto* texture = boundTexture2D();
    if (!texture || !texture->storageCreated || texture->levels.empty()
        || texture->definedLevels.empty() || !texture->definedLevels[0]) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    if (!regenerateTextureMipmaps(*texture))
        addError(GL_OUT_OF_MEMORY_VALUE);
}

void GraphicsContextGLWKC::attachShader(PlatformGLObject p,PlatformGLObject s)
{
    auto pi=m_programs.find(p);
    auto shader=m_shaders.find(s);
    if(pi==m_programs.end() || shader==m_shaders.end() || shader->second.deletePending) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }
    if(pi->second.linked) { addError(GL_INVALID_OPERATION_VALUE); return; }
    auto& attached=pi->second.shaders;
    if(std::find(attached.begin(),attached.end(),s)==attached.end()) attached.push_back(s);
}
void GraphicsContextGLWKC::bindAttribLocation(PlatformGLObject p,GCGLuint index,const CString& name)
{
    auto pi=m_programs.find(p);
    if(pi==m_programs.end() || name.isNull()) { addError(GL_INVALID_VALUE_VALUE); return; }
    if(index>=32) { addError(GL_INVALID_VALUE_VALUE); return; }
    if(pi->second.linked) { addError(GL_INVALID_OPERATION_VALUE); return; }
    pi->second.requestedLocations[std::string(name.data(),name.length())]=index;
}
void GraphicsContextGLWKC::bindBuffer(GCGLenum target,PlatformGLObject name)
{
    WKC::GLGX2::BufferTarget mapped;
    if(!mapBufferTarget(target,mapped)) { addError(GL_INVALID_ENUM_VALUE); return; }
    if(m_buffers.bind(mapped,name)!=WKC::GLGX2::BufferResult::Ok) { addError(GL_INVALID_OPERATION_VALUE); return; }
    if(mapped==WKC::GLGX2::BufferTarget::ElementArray &&
       m_vertexArrays.setElementBuffer(name,m_buffers)!=WKC::GLGX2::VertexArrayResult::Ok) addError(GL_INVALID_OPERATION_VALUE);
}
void GraphicsContextGLWKC::bindFramebuffer(GCGLenum target,PlatformGLObject name)
{
    if(target!=GL_FRAMEBUFFER_VALUE) { addError(GL_INVALID_ENUM_VALUE); return; }
    if(name) { addError(GL_INVALID_OPERATION_VALUE); return; }
    m_boundFramebuffer=0; WKC::GLGX2::bindWUTRenderTarget(m_native);
}
void GraphicsContextGLWKC::blendColor(GCGLclampf r,GCGLclampf g,GCGLclampf b,GCGLclampf a)
{ addDriverError(m_driver.setBlendColor({r,g,b,a}),"blendColor"); }
void GraphicsContextGLWKC::blendEquation(GCGLenum mode) { blendEquationSeparate(mode,mode); }
void GraphicsContextGLWKC::blendEquationSeparate(GCGLenum rgb,GCGLenum alpha)
{
    auto map=[](GCGLenum v,WKC::GLGX2::BlendEquation& out) {
        switch(v) { case 0x8006:out=WKC::GLGX2::BlendEquation::Add;return true;
        case 0x800A:out=WKC::GLGX2::BlendEquation::Subtract;return true;
        case 0x800B:out=WKC::GLGX2::BlendEquation::ReverseSubtract;return true;
        case 0x8007:out=WKC::GLGX2::BlendEquation::Minimum;return true;
        case 0x8008:out=WKC::GLGX2::BlendEquation::Maximum;return true;default:return false; }
    };
    WKC::GLGX2::BlendEquation r,a;
    if(!map(rgb,r)||!map(alpha,a)) { addError(GL_INVALID_ENUM_VALUE); return; }
    addDriverError(m_driver.setBlendEquation(r,a),"blendEquationSeparate");
}
void GraphicsContextGLWKC::blendFunc(GCGLenum s,GCGLenum d) { blendFuncSeparate(s,d,s,d); }
void GraphicsContextGLWKC::blendFuncSeparate(GCGLenum sr,GCGLenum dr,GCGLenum sa,GCGLenum da)
{
    auto map=[](GCGLenum v,WKC::GLGX2::BlendFactor& out) {
        using WKC::GLGX2::BlendFactor;
        switch(v) {
        case 0:out=BlendFactor::Zero;return true;case 1:out=BlendFactor::One;return true;
        case 0x0300:out=BlendFactor::SourceColor;return true;case 0x0301:out=BlendFactor::OneMinusSourceColor;return true;
        case 0x0306:out=BlendFactor::DestinationColor;return true;case 0x0307:out=BlendFactor::OneMinusDestinationColor;return true;
        case 0x0302:out=BlendFactor::SourceAlpha;return true;case 0x0303:out=BlendFactor::OneMinusSourceAlpha;return true;
        case 0x0304:out=BlendFactor::DestinationAlpha;return true;case 0x0305:out=BlendFactor::OneMinusDestinationAlpha;return true;
        case 0x8001:out=BlendFactor::ConstantColor;return true;case 0x8002:out=BlendFactor::OneMinusConstantColor;return true;
        case 0x8003:out=BlendFactor::ConstantAlpha;return true;case 0x8004:out=BlendFactor::OneMinusConstantAlpha;return true;
        case 0x0308:out=BlendFactor::SourceAlphaSaturate;return true;default:return false;
        }
    };
    WKC::GLGX2::BlendFactor sRGB,dRGB,sAlpha,dAlpha;
    if(!map(sr,sRGB)||!map(dr,dRGB)||!map(sa,sAlpha)||!map(da,dAlpha)) { addError(GL_INVALID_ENUM_VALUE); return; }
    addDriverError(m_driver.setBlendFunction(sRGB,dRGB,sAlpha,dAlpha),"blendFuncSeparate");
}
GCGLenum GraphicsContextGLWKC::checkFramebufferStatus(GCGLenum target)
{
    if(target!=GL_FRAMEBUFFER_VALUE) { addError(GL_INVALID_ENUM_VALUE); return 0; }
    return m_boundFramebuffer ? GL_FRAMEBUFFER_UNSUPPORTED_VALUE : GL_FRAMEBUFFER_COMPLETE_VALUE;
}
void GraphicsContextGLWKC::clear(GCGLbitfield mask)
{
    uint8_t translated=0;
    if(mask&GL_COLOR_BUFFER_BIT_VALUE) translated|=WKC::GLGX2::ClearColorBuffer;
    if(mask&GL_DEPTH_BUFFER_BIT_VALUE) translated|=WKC::GLGX2::ClearDepthBuffer;
    if(mask&GL_STENCIL_BUFFER_BIT_VALUE) translated|=WKC::GLGX2::ClearStencilBuffer;
    if(mask&~(GL_COLOR_BUFFER_BIT_VALUE|GL_DEPTH_BUFFER_BIT_VALUE|GL_STENCIL_BUFFER_BIT_VALUE)) { addError(GL_INVALID_VALUE_VALUE);return; }
    addDriverError(m_driver.clear(translated),"clear");
}
void GraphicsContextGLWKC::clearColor(GCGLclampf r,GCGLclampf g,GCGLclampf b,GCGLclampf a) { addDriverError(m_driver.setClearColor({r,g,b,a}),"clearColor"); }
void GraphicsContextGLWKC::clearDepth(GCGLclampf depth) { addDriverError(m_driver.setClearDepth(depth),"clearDepth"); }
void GraphicsContextGLWKC::clearStencil(GCGLint v) { addDriverError(m_driver.setClearStencil(v),"clearStencil"); }
void GraphicsContextGLWKC::colorMask(GCGLboolean r,GCGLboolean g,GCGLboolean b,GCGLboolean a) { addDriverError(m_driver.setColorMask({!!r,!!g,!!b,!!a}),"colorMask"); }
void GraphicsContextGLWKC::compileShader(PlatformGLObject name)
{
    auto it=m_shaders.find(name); if(it==m_shaders.end()) { addError(GL_INVALID_VALUE_VALUE);return; }
    auto& s=it->second;
    FreeVertexShader(s.compiledVertex); FreePixelShader(s.compiledPixel);
    s.compiledVertex=nullptr;s.compiledPixel=nullptr;s.compiled=false;s.log.clear();
    if(!m_compilerAvailable||s.source.empty()) { s.log=m_compilerAvailable?"Shader source is empty.":"CafeGLSL runtime compiler is unavailable.";return; }
    std::array<char,4096> log{};
    if(s.type==GL_VERTEX_SHADER_VALUE)
        s.compiledMode=CompileVertexShader(s.source.c_str(),GLSL_COMPILE_REGISTER,&s.compiledVertex,log.data(),log.size(),GLSL_COMPILER_FLAG_NONE);
    else if(s.type==GL_FRAGMENT_SHADER_VALUE)
        s.compiledMode=CompilePixelShader(s.source.c_str(),GLSL_COMPILE_REGISTER,&s.compiledPixel,log.data(),log.size(),GLSL_COMPILER_FLAG_NONE);
    else { s.log="Only WebGL 1 vertex and fragment shaders are supported.";return; }
    s.log=log.data(); s.compiled=s.compiledMode!=GLSL_SHADER_MODE_ERROR&&(s.compiledVertex||s.compiledPixel);
}
PlatformGLObject GraphicsContextGLWKC::createBuffer()
{
    WKC::GLGX2::BufferName n=0;
    if(m_buffers.create(n)!=WKC::GLGX2::BufferResult::Ok) { addError(GL_OUT_OF_MEMORY_VALUE);return 0; }
    return n;
}
PlatformGLObject GraphicsContextGLWKC::createProgram()
{
    auto n=nextProgramName(); if(!n) { addError(GL_OUT_OF_MEMORY_VALUE);return 0; }
    m_programs.emplace(n,ProgramRecord{}); return n;
}
PlatformGLObject GraphicsContextGLWKC::createShader(GCGLenum type)
{
    if(type!=GL_VERTEX_SHADER_VALUE&&type!=GL_FRAGMENT_SHADER_VALUE) { addError(GL_INVALID_ENUM_VALUE);return 0; }
    auto n=nextShaderName(); if(!n) { addError(GL_OUT_OF_MEMORY_VALUE);return 0; }
    ShaderRecord s;s.type=type;m_shaders.emplace(n,std::move(s));return n;
}
void GraphicsContextGLWKC::cullFace(GCGLenum mode)
{
    WKC::GLGX2::Face f;
    if(mode==0x0404) f=WKC::GLGX2::Face::Front;
    else if(mode==0x0405) f=WKC::GLGX2::Face::Back;
    else if(mode==0x0408) f=WKC::GLGX2::Face::FrontAndBack;
    else { addError(GL_INVALID_ENUM_VALUE);return; }
    addDriverError(m_driver.setCullFace(f),"cullFace");
}
void GraphicsContextGLWKC::deleteBuffer(PlatformGLObject name)
{
    if(!name)return;
    if(m_buffers.destroy(name)!=WKC::GLGX2::BufferResult::Ok) addError(GL_INVALID_OPERATION_VALUE);
}
void GraphicsContextGLWKC::deleteProgram(PlatformGLObject name)
{
    auto it=m_programs.find(name);
    if(it==m_programs.end())
        return;
    if(m_currentProgram==name) {
        m_currentProgram=0;
        clearActiveNativeProgram();
    }
    // Removing a program detaches its shaders. Delete-pending shader objects
    // can be reclaimed only after their final program reference disappears.
    const auto attachedShaders=it->second.shaders;
    releaseProgramNative(it->second);
    m_programs.erase(it);
    for(auto shaderName:attachedShaders)
        releaseShaderIfUnused(shaderName);
}
void GraphicsContextGLWKC::releaseShaderIfUnused(PlatformGLObject name)
{
    auto shader=m_shaders.find(name);
    if(shader==m_shaders.end() || !shader->second.deletePending)
        return;
    for(const auto& program:m_programs) {
        if(std::find(program.second.shaders.begin(),program.second.shaders.end(),name)
            !=program.second.shaders.end())
            return;
    }
    FreeVertexShader(shader->second.compiledVertex);
    FreePixelShader(shader->second.compiledPixel);
    m_shaders.erase(shader);
}

void GraphicsContextGLWKC::deleteShader(PlatformGLObject name)
{
    auto it=m_shaders.find(name);
    if(it==m_shaders.end())
        return;
    it->second.deletePending=true;
    releaseShaderIfUnused(name);
}
void GraphicsContextGLWKC::depthFunc(GCGLenum v)
{
    WKC::GLGX2::CompareFunction out;
    switch(v){case 0x0200:out=WKC::GLGX2::CompareFunction::Never;break;case 0x0201:out=WKC::GLGX2::CompareFunction::Less;break;
    case 0x0202:out=WKC::GLGX2::CompareFunction::Equal;break;case 0x0203:out=WKC::GLGX2::CompareFunction::LessEqual;break;
    case 0x0204:out=WKC::GLGX2::CompareFunction::Greater;break;case 0x0205:out=WKC::GLGX2::CompareFunction::NotEqual;break;
    case 0x0206:out=WKC::GLGX2::CompareFunction::GreaterEqual;break;case 0x0207:out=WKC::GLGX2::CompareFunction::Always;break;
    default:addError(GL_INVALID_ENUM_VALUE);return;}
    addDriverError(m_driver.setDepthFunction(out),"depthFunc");
}
void GraphicsContextGLWKC::depthMask(GCGLboolean f){addDriverError(m_driver.setDepthWrite(f),"depthMask");}
void GraphicsContextGLWKC::depthRange(GCGLclampf n,GCGLclampf f){addDriverError(m_driver.setDepthRange(n,f),"depthRange");}
void GraphicsContextGLWKC::detachShader(PlatformGLObject p,PlatformGLObject s)
{
    auto i=m_programs.find(p);if(i==m_programs.end()){addError(GL_INVALID_VALUE_VALUE);return;}
    auto& a=i->second.shaders;
    auto attached=std::find(a.begin(),a.end(),s);
    if(attached==a.end()) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    a.erase(attached);
    releaseShaderIfUnused(s);
}
void GraphicsContextGLWKC::disable(GCGLenum cap)
{
    auto map=[](GCGLenum v)->std::optional<WKC::GLGX2::Capability>{
        switch(v){case GL_BLEND_VALUE:return WKC::GLGX2::Capability::Blend;case GL_CULL_FACE_VALUE:return WKC::GLGX2::Capability::CullFace;
        case GL_DEPTH_TEST_VALUE:return WKC::GLGX2::Capability::DepthTest;case GL_SCISSOR_TEST_VALUE:return WKC::GLGX2::Capability::ScissorTest;
        case GL_STENCIL_TEST_VALUE:return WKC::GLGX2::Capability::StencilTest;case GL_POLYGON_OFFSET_FILL_VALUE:return WKC::GLGX2::Capability::PolygonOffsetFill;default:return std::nullopt;}
    }(cap);
    if(!map){addError(GL_INVALID_ENUM_VALUE);return;}addDriverError(m_driver.disable(*map),"disable");
}
void GraphicsContextGLWKC::disableVertexAttribArray(GCGLuint i)
{if(i>=32||m_vertexArrays.enableAttribute(i,false)!=WKC::GLGX2::VertexArrayResult::Ok)addError(i>=32?GL_INVALID_VALUE_VALUE:GL_INVALID_OPERATION_VALUE);}
void GraphicsContextGLWKC::drawArrays(GCGLenum mode,GCGLint first,GCGLsizei count)
{
    WKC::GLGX2::Primitive p;if(!mapPrimitive(mode,p)){addError(GL_INVALID_ENUM_VALUE);return;}
    if(first<0||count<0){addError(GL_INVALID_VALUE_VALUE);return;}if(!activateProgram(m_currentProgram))return;
    addDriverError(m_driver.drawArrays(p,first,count),"drawArrays");
}
void GraphicsContextGLWKC::drawElements(GCGLenum mode,GCGLsizei count,GCGLenum type,GCGLintptr offset)
{
    WKC::GLGX2::Primitive p;WKC::GLGX2::IndexType t;
    if(!mapPrimitive(mode,p)||!mapIndexType(type,t)){addError(GL_INVALID_ENUM_VALUE);return;}
    if(count<0||offset<0){addError(GL_INVALID_VALUE_VALUE);return;}if(!activateProgram(m_currentProgram))return;
    addDriverError(m_driver.drawElements(p,count,t,static_cast<uint64_t>(offset)),"drawElements");
}
void GraphicsContextGLWKC::drawArraysInstanced(GCGLenum mode,GCGLint first,GCGLsizei count,GCGLsizei instances)
{
    WKC::GLGX2::Primitive p;if(!mapPrimitive(mode,p)){addError(GL_INVALID_ENUM_VALUE);return;}
    if(first<0||count<0||instances<0){addError(GL_INVALID_VALUE_VALUE);return;}if(!activateProgram(m_currentProgram))return;
    addDriverError(m_driver.drawArraysInstanced(p,first,count,instances),"drawArraysInstanced");
}
void GraphicsContextGLWKC::drawElementsInstanced(GCGLenum mode,GCGLsizei count,GCGLenum type,GCGLintptr offset,GCGLsizei instances)
{
    WKC::GLGX2::Primitive p;WKC::GLGX2::IndexType t;
    if(!mapPrimitive(mode,p)||!mapIndexType(type,t)){addError(GL_INVALID_ENUM_VALUE);return;}
    if(count<0||offset<0||instances<0){addError(GL_INVALID_VALUE_VALUE);return;}if(!activateProgram(m_currentProgram))return;
    addDriverError(m_driver.drawElementsInstanced(p,count,t,offset,instances),"drawElementsInstanced");
}
void GraphicsContextGLWKC::enable(GCGLenum cap)
{
    auto map=[](GCGLenum v)->std::optional<WKC::GLGX2::Capability>{
        switch(v){case GL_BLEND_VALUE:return WKC::GLGX2::Capability::Blend;case GL_CULL_FACE_VALUE:return WKC::GLGX2::Capability::CullFace;
        case GL_DEPTH_TEST_VALUE:return WKC::GLGX2::Capability::DepthTest;case GL_SCISSOR_TEST_VALUE:return WKC::GLGX2::Capability::ScissorTest;
        case GL_STENCIL_TEST_VALUE:return WKC::GLGX2::Capability::StencilTest;case GL_POLYGON_OFFSET_FILL_VALUE:return WKC::GLGX2::Capability::PolygonOffsetFill;default:return std::nullopt;}
    }(cap);
    if(!map){addError(GL_INVALID_ENUM_VALUE);return;}addDriverError(m_driver.enable(*map),"enable");
}
void GraphicsContextGLWKC::enableVertexAttribArray(GCGLuint i)
{if(i>=32||m_vertexArrays.enableAttribute(i,true)!=WKC::GLGX2::VertexArrayResult::Ok)addError(i>=32?GL_INVALID_VALUE_VALUE:GL_INVALID_OPERATION_VALUE);}
void GraphicsContextGLWKC::finish(){GX2DrawDone();}
void GraphicsContextGLWKC::flush(){GX2DrawDone();}
void GraphicsContextGLWKC::frontFace(GCGLenum m)
{
    WKC::GLGX2::Winding w;if(m==0x0900)w=WKC::GLGX2::Winding::Clockwise;else if(m==0x0901)w=WKC::GLGX2::Winding::CounterClockwise;
    else{addError(GL_INVALID_ENUM_VALUE);return;}addDriverError(m_driver.setFrontFace(w),"frontFace");
}
Vector<GCGLAttribActiveInfo> GraphicsContextGLWKC::activeAttribs(PlatformGLObject name)
{
    Vector<GCGLAttribActiveInfo> out;auto p=m_programs.find(name);if(p==m_programs.end()||!p->second.linked)return out;
    for(const auto& a:p->second.attributes){GCGLAttribActiveInfo i;i.name=CString(a.name.c_str());i.type=a.type;i.location=a.location;out.append(WTFMove(i));}
    return out;
}
Vector<GCGLUniformActiveInfo> GraphicsContextGLWKC::activeUniforms(PlatformGLObject name)
{
    Vector<GCGLUniformActiveInfo> out;auto p=m_programs.find(name);if(p==m_programs.end()||!p->second.linked)return out;
    for(const auto& u:p->second.uniforms){GCGLUniformActiveInfo i;i.name=CString(u.name.c_str());i.type=u.type;i.size=u.size;
        for(const auto& b:u.elements)i.locations.append(b.location);out.append(WTFMove(i));}return out;
}
GCGLint GraphicsContextGLWKC::getBufferParameteri(GCGLenum target,GCGLenum pname)
{
    WKC::GLGX2::BufferTarget mapped;if(!mapBufferTarget(target,mapped)){addError(GL_INVALID_ENUM_VALUE);return 0;}
    auto n=m_buffers.bound(mapped);WKC::GLGX2::BufferInfo info;if(!n||!m_buffers.info(n,info)){addError(GL_INVALID_OPERATION_VALUE);return 0;}
    if(pname==GL_BUFFER_SIZE_VALUE)return static_cast<GCGLint>(std::min<size_t>(info.size,INT32_MAX));
    if(pname==GL_BUFFER_USAGE_VALUE){switch(info.usage){case WKC::GLGX2::BufferUsage::StaticDraw:return GL_STATIC_DRAW_VALUE;
    case WKC::GLGX2::BufferUsage::DynamicDraw:return GL_DYNAMIC_DRAW_VALUE;case WKC::GLGX2::BufferUsage::StreamDraw:return GL_STREAM_DRAW_VALUE;}}
    addError(GL_INVALID_ENUM_VALUE);return 0;
}
CString GraphicsContextGLWKC::getString(GCGLenum n)
{
    switch(n){case GL_VENDOR_VALUE:return CString("Nintendo/WKC");case GL_RENDERER_VALUE:return CString("Wii U GX2");
    case GL_VERSION_VALUE:return CString("WebGL 1.0 (GX2)");case GL_SHADING_LANGUAGE_VERSION_VALUE:return CString("OpenGL ES GLSL ES 1.00 (CafeGLSL)");
    case GL_EXTENSIONS_VALUE:return CString("");default:addError(GL_INVALID_ENUM_VALUE);return CString();}
}
void GraphicsContextGLWKC::getFloatv(GCGLenum n,std::span<GCGLfloat> v)
{
    std::fill(v.begin(),v.end(),0.0f);
    switch(n){case GL_COLOR_CLEAR_VALUE_VALUE:if(v.size()>=4){v[0]=m_native.clearColor.red;v[1]=m_native.clearColor.green;v[2]=m_native.clearColor.blue;v[3]=m_native.clearColor.alpha;}return;
    case GL_DEPTH_CLEAR_VALUE_VALUE:if(!v.empty())v[0]=m_native.clearDepth;return;case GL_DEPTH_RANGE_VALUE:if(v.size()>=2){v[0]=m_native.viewport.nearDepth;v[1]=m_native.viewport.farDepth;}return;
    default:addError(GL_INVALID_ENUM_VALUE);return;}
}
void GraphicsContextGLWKC::getIntegerv(GCGLenum n,std::span<GCGLint> v)
{
    std::fill(v.begin(),v.end(),0);if(v.empty())return;
    switch(n){case GL_ARRAY_BUFFER_BINDING_VALUE:v[0]=m_buffers.bound(WKC::GLGX2::BufferTarget::Array);return;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING_VALUE:v[0]=m_vertexArrays.elementBuffer();return;
    case GL_FRAMEBUFFER_BINDING_VALUE:v[0]=m_boundFramebuffer;return;
    case GL_TEXTURE_BINDING_2D_VALUE:v[0]=m_boundTextures2D[m_activeTextureUnit];return;
    case GL_MAX_VERTEX_ATTRIBS_VALUE:v[0]=maxVertexAttribs();return;case GL_MAX_TEXTURE_SIZE_VALUE:v[0]=maxTextureSize();return;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE_VALUE:v[0]=maxCubeMapTextureSize();return;case GL_MAX_RENDERBUFFER_SIZE_VALUE:v[0]=maxRenderbufferSize();return;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_VALUE:v[0]=maxCombinedTextureImageUnits();return;
    case GL_VIEWPORT_VALUE:if(v.size()>=4){v[0]=m_native.viewport.x;v[1]=m_native.viewport.y;v[2]=m_native.viewport.width;v[3]=m_native.viewport.height;}return;
    default:addError(GL_INVALID_ENUM_VALUE);return;}
}
GCGLint GraphicsContextGLWKC::getProgrami(PlatformGLObject n,GCGLenum what)
{
    auto p=m_programs.find(n);if(p==m_programs.end()){addError(GL_INVALID_VALUE_VALUE);return 0;}
    switch(what){case GL_LINK_STATUS_VALUE:return p->second.linked;case GL_VALIDATE_STATUS_VALUE:return p->second.validated;
    case GL_DELETE_STATUS_VALUE:return p->second.deletePending;case GL_ATTACHED_SHADERS_VALUE:return p->second.shaders.size();
    case GL_ACTIVE_ATTRIBUTES_VALUE:return p->second.attributes.size();case GL_ACTIVE_UNIFORMS_VALUE:return p->second.uniforms.size();
    case GL_INFO_LOG_LENGTH_VALUE:return p->second.log.size()+1;default:addError(GL_INVALID_ENUM_VALUE);return 0;}
}
CString GraphicsContextGLWKC::getProgramInfoLog(PlatformGLObject n)
{auto p=m_programs.find(n);if(p==m_programs.end()){addError(GL_INVALID_VALUE_VALUE);return CString();}return CString(p->second.log.c_str());}
GCGLint GraphicsContextGLWKC::getShaderi(PlatformGLObject n,GCGLenum what)
{
    auto s=m_shaders.find(n);if(s==m_shaders.end()){addError(GL_INVALID_VALUE_VALUE);return 0;}
    switch(what){case GL_COMPILE_STATUS_VALUE:return s->second.compiled;case GL_SHADER_TYPE_VALUE:return s->second.type;
    case GL_DELETE_STATUS_VALUE:return s->second.deletePending;case GL_INFO_LOG_LENGTH_VALUE:return s->second.log.size()+1;
    case GL_SHADER_SOURCE_LENGTH_VALUE:return s->second.source.size()+1;default:addError(GL_INVALID_ENUM_VALUE);return 0;}
}
CString GraphicsContextGLWKC::getShaderInfoLog(PlatformGLObject n)
{auto p=m_shaders.find(n);if(p==m_shaders.end()){addError(GL_INVALID_VALUE_VALUE);return CString();}return CString(p->second.log.c_str());}
void GraphicsContextGLWKC::getUniformfv(PlatformGLObject programName, GCGLint location,
    std::span<GCGLfloat> output)
{
    std::fill(output.begin(), output.end(), 0.0f);
    auto program = m_programs.find(programName);
    if (program == m_programs.end() || !program->second.linked) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }
    auto found = program->second.uniformLocations.find(location);
    if (found == program->second.uniformLocations.end()) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    const auto& uniform = program->second.uniforms[found->second.uniformIndex];
    const auto& binding = uniform.elements[found->second.elementIndex];
    if (!binding.valueSet)
        return;
    const size_t count = std::min(output.size(), static_cast<size_t>(uniformComponents(uniform.type)));
    const bool integerType = uniform.type == GL_INT_VALUE || uniform.type == GL_BOOL_VALUE
        || uniform.type == GL_INT_VEC2_VALUE || uniform.type == GL_INT_VEC3_VALUE || uniform.type == GL_INT_VEC4_VALUE
        || uniform.type == GL_BOOL_VEC2_VALUE || uniform.type == GL_BOOL_VEC3_VALUE || uniform.type == GL_BOOL_VEC4_VALUE
        || uniform.type == GL_SAMPLER_2D_VALUE || uniform.type == GL_SAMPLER_CUBE_VALUE;
    for (size_t i = 0; i < count; ++i) {
        if (integerType) {
            if ((i + 1) * sizeof(int32_t) > binding.value.size()) break;
            int32_t value;
            std::memcpy(&value, binding.value.data() + i * sizeof(value), sizeof(value));
            output[i] = static_cast<GCGLfloat>(value);
        } else {
            if ((i + 1) * sizeof(GCGLfloat) > binding.value.size()) break;
            std::memcpy(&output[i], binding.value.data() + i * sizeof(GCGLfloat), sizeof(GCGLfloat));
        }
    }
}

void GraphicsContextGLWKC::getUniformiv(PlatformGLObject programName, GCGLint location,
    std::span<GCGLint> output)
{
    std::fill(output.begin(), output.end(), 0);
    auto program = m_programs.find(programName);
    if (program == m_programs.end() || !program->second.linked) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }
    auto found = program->second.uniformLocations.find(location);
    if (found == program->second.uniformLocations.end()) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    const auto& uniform = program->second.uniforms[found->second.uniformIndex];
    const auto& binding = uniform.elements[found->second.elementIndex];
    if (!binding.valueSet)
        return;
    const size_t count = std::min(output.size(), static_cast<size_t>(uniformComponents(uniform.type)));
    const bool integerType = uniform.type == GL_INT_VALUE || uniform.type == GL_BOOL_VALUE
        || uniform.type == GL_INT_VEC2_VALUE || uniform.type == GL_INT_VEC3_VALUE || uniform.type == GL_INT_VEC4_VALUE
        || uniform.type == GL_BOOL_VEC2_VALUE || uniform.type == GL_BOOL_VEC3_VALUE || uniform.type == GL_BOOL_VEC4_VALUE
        || uniform.type == GL_SAMPLER_2D_VALUE || uniform.type == GL_SAMPLER_CUBE_VALUE;
    for (size_t i = 0; i < count; ++i) {
        if (integerType) {
            if ((i + 1) * sizeof(int32_t) > binding.value.size()) break;
            std::memcpy(&output[i], binding.value.data() + i * sizeof(int32_t), sizeof(int32_t));
        } else {
            GCGLfloat value = 0;
            if ((i + 1) * sizeof(value) > binding.value.size()) break;
            std::memcpy(&value, binding.value.data() + i * sizeof(value), sizeof(value));
            output[i] = static_cast<GCGLint>(value);
        }
    }
}

void GraphicsContextGLWKC::getUniformuiv(PlatformGLObject programName, GCGLint location,
    std::span<GCGLuint> output)
{
    std::fill(output.begin(), output.end(), 0);
    auto program = m_programs.find(programName);
    if (program == m_programs.end() || !program->second.linked) {
        addError(GL_INVALID_VALUE_VALUE);
        return;
    }
    auto found = program->second.uniformLocations.find(location);
    if (found == program->second.uniformLocations.end()) {
        addError(GL_INVALID_OPERATION_VALUE);
        return;
    }
    const auto& uniform = program->second.uniforms[found->second.uniformIndex];
    const auto& binding = uniform.elements[found->second.elementIndex];
    if (!binding.valueSet)
        return;
    const size_t count = std::min(output.size(), static_cast<size_t>(uniformComponents(uniform.type)));
    const bool integerType = uniform.type == GL_INT_VALUE || uniform.type == GL_BOOL_VALUE
        || uniform.type == GL_INT_VEC2_VALUE || uniform.type == GL_INT_VEC3_VALUE || uniform.type == GL_INT_VEC4_VALUE
        || uniform.type == GL_BOOL_VEC2_VALUE || uniform.type == GL_BOOL_VEC3_VALUE || uniform.type == GL_BOOL_VEC4_VALUE
        || uniform.type == GL_SAMPLER_2D_VALUE || uniform.type == GL_SAMPLER_CUBE_VALUE;
    for (size_t i = 0; i < count; ++i) {
        if (integerType) {
            if ((i + 1) * sizeof(int32_t) > binding.value.size()) break;
            int32_t value;
            std::memcpy(&value, binding.value.data() + i * sizeof(value), sizeof(value));
            output[i] = static_cast<GCGLuint>(value);
        } else {
            GCGLfloat value = 0;
            if ((i + 1) * sizeof(value) > binding.value.size()) break;
            std::memcpy(&value, binding.value.data() + i * sizeof(value), sizeof(value));
            output[i] = static_cast<GCGLuint>(std::max<GCGLfloat>(0, value));
        }
    }
}

GCGLErrorCodeSet GraphicsContextGLWKC::getErrors(){auto e=m_errors;m_errors={};return e;}
void GraphicsContextGLWKC::linkProgram(PlatformGLObject n)
{
    auto it=m_programs.find(n);if(it==m_programs.end()){addError(GL_INVALID_VALUE_VALUE);return;}
    auto& p=it->second;p.log.clear();p.linked=false;p.validated=false;releaseProgramNative(p);
    ShaderRecord* vs=nullptr;ShaderRecord* ps=nullptr;
    for(auto sname:p.shaders) {
        auto s=m_shaders.find(sname);
        if(s==m_shaders.end() || !s->second.compiled) {
            p.log="Every attached shader must compile successfully before linking.";
            return;
        }
        if(s->second.type==GL_VERTEX_SHADER_VALUE)
            vs=&s->second;
        if(s->second.type==GL_FRAGMENT_SHADER_VALUE)
            ps=&s->second;
    }
    if(!vs||!ps){p.log="A graphics program requires vertex and fragment shaders.";return;}
    std::array<char,4096> log{};GX2VertexShader* vertex=nullptr;GX2PixelShader* pixel=nullptr;
    auto mode=CompileShaderPair(vs->source.c_str(),ps->source.c_str(),GLSL_COMPILE_REGISTER,&vertex,&pixel,log.data(),log.size(),GLSL_COMPILER_FLAG_NONE);
    p.log=log.data();if(mode==GLSL_SHADER_MODE_ERROR||!vertex||!pixel){FreeShaders(vertex,pixel);return;}
    p.vertexShader=vertex;p.pixelShader=pixel;p.mode=mode;p.linked=true;buildProgramInterface(p);
    if(!p.linked){releaseProgramNative(p);return;}if(m_currentProgram==n)activateProgram(n);
}
void GraphicsContextGLWKC::shaderSource(PlatformGLObject n,const CString& src)
{
    auto i=m_shaders.find(n);if(i==m_shaders.end()||i->second.deletePending||src.isNull()){addError(GL_INVALID_VALUE_VALUE);return;}
    auto& s=i->second;FreeVertexShader(s.compiledVertex);FreePixelShader(s.compiledPixel);s.compiledVertex=nullptr;s.compiledPixel=nullptr;
    s.source.assign(src.data(),src.length());s.compiled=false;s.log.clear();
}
void GraphicsContextGLWKC::useProgram(PlatformGLObject n){if(!n){activateProgram(0);return;}activateProgram(n);}
void GraphicsContextGLWKC::validateProgram(PlatformGLObject n){auto p=m_programs.find(n);if(p==m_programs.end()){addError(GL_INVALID_VALUE_VALUE);return;}p->second.validated=p->second.linked;}
void GraphicsContextGLWKC::uniform1f(GCGLint l,GCGLfloat a){float v[]={a};setUniformData(l,v,sizeof(v),1,1,false);}
void GraphicsContextGLWKC::uniform1fv(GCGLint l,std::span<const GCGLfloat> v){setUniformData(l,v.data(),v.size_bytes(),1,v.size(),false);}
void GraphicsContextGLWKC::uniform1i(GCGLint l,GCGLint a){int32_t v[]={a};setUniformData(l,v,sizeof(v),1,1,true);}
void GraphicsContextGLWKC::uniform1iv(GCGLint l,std::span<const GCGLint> v){setUniformData(l,v.data(),v.size_bytes(),1,v.size(),true);}
void GraphicsContextGLWKC::uniform2f(GCGLint l,GCGLfloat a,GCGLfloat b){float v[]={a,b};setUniformData(l,v,sizeof(v),2,1,false);}
void GraphicsContextGLWKC::uniform2fv(GCGLint l,std::span<const GCGLfloat> v){if(v.size()%2){addError(GL_INVALID_VALUE_VALUE);return;}setUniformData(l,v.data(),v.size_bytes(),2,v.size()/2,false);}
void GraphicsContextGLWKC::uniform2i(GCGLint l,GCGLint a,GCGLint b){int32_t v[]={a,b};setUniformData(l,v,sizeof(v),2,1,true);}
void GraphicsContextGLWKC::uniform2iv(GCGLint l,std::span<const GCGLint> v){if(v.size()%2){addError(GL_INVALID_VALUE_VALUE);return;}setUniformData(l,v.data(),v.size_bytes(),2,v.size()/2,true);}
void GraphicsContextGLWKC::uniform3f(GCGLint l,GCGLfloat a,GCGLfloat b,GCGLfloat c){float v[]={a,b,c};setUniformData(l,v,sizeof(v),3,1,false);}
void GraphicsContextGLWKC::uniform3fv(GCGLint l,std::span<const GCGLfloat> v){if(v.size()%3){addError(GL_INVALID_VALUE_VALUE);return;}setUniformData(l,v.data(),v.size_bytes(),3,v.size()/3,false);}
void GraphicsContextGLWKC::uniform3i(GCGLint l,GCGLint a,GCGLint b,GCGLint c){int32_t v[]={a,b,c};setUniformData(l,v,sizeof(v),3,1,true);}
void GraphicsContextGLWKC::uniform3iv(GCGLint l,std::span<const GCGLint> v){if(v.size()%3){addError(GL_INVALID_VALUE_VALUE);return;}setUniformData(l,v.data(),v.size_bytes(),3,v.size()/3,true);}
void GraphicsContextGLWKC::uniform4f(GCGLint l,GCGLfloat a,GCGLfloat b,GCGLfloat c,GCGLfloat d){float v[]={a,b,c,d};setUniformData(l,v,sizeof(v),4,1,false);}
void GraphicsContextGLWKC::uniform4fv(GCGLint l,std::span<const GCGLfloat> v){if(v.size()%4){addError(GL_INVALID_VALUE_VALUE);return;}setUniformData(l,v.data(),v.size_bytes(),4,v.size()/4,false);}
void GraphicsContextGLWKC::uniform4i(GCGLint l,GCGLint a,GCGLint b,GCGLint c,GCGLint d){int32_t v[]={a,b,c,d};setUniformData(l,v,sizeof(v),4,1,true);}
void GraphicsContextGLWKC::uniform4iv(GCGLint l,std::span<const GCGLint> v){if(v.size()%4){addError(GL_INVALID_VALUE_VALUE);return;}setUniformData(l,v.data(),v.size_bytes(),4,v.size()/4,true);}
void GraphicsContextGLWKC::uniformMatrix2fv(GCGLint l,GCGLboolean t,std::span<const GCGLfloat> v){if(t||v.size()%4){addError(GL_INVALID_VALUE_VALUE);return;}setUniformData(l,v.data(),v.size_bytes(),4,v.size()/4,false,2);}
void GraphicsContextGLWKC::uniformMatrix3fv(GCGLint l,GCGLboolean t,std::span<const GCGLfloat> v){if(t||v.size()%9){addError(GL_INVALID_VALUE_VALUE);return;}setUniformData(l,v.data(),v.size_bytes(),9,v.size()/9,false,3);}
void GraphicsContextGLWKC::uniformMatrix4fv(GCGLint l,GCGLboolean t,std::span<const GCGLfloat> v){if(t||v.size()%16){addError(GL_INVALID_VALUE_VALUE);return;}setUniformData(l,v.data(),v.size_bytes(),16,v.size()/16,false,4);}
void GraphicsContextGLWKC::vertexAttribPointer(GCGLuint i,GCGLint size,GCGLenum type,GCGLboolean norm,GCGLsizei stride,GCGLintptr offset)
{
    WKC::GLGX2::AttributeType t;if(i>=32||size<1||size>4||stride<0||offset<0||!mapAttributeType(type,t)){addError(GL_INVALID_VALUE_VALUE);return;}
    auto buffer=m_buffers.bound(WKC::GLGX2::BufferTarget::Array);if(!buffer){addError(GL_INVALID_OPERATION_VALUE);return;}
    WKC::GLGX2::VertexAttribute a;a.buffer=buffer;a.components=size;a.type=t;a.normalized=norm;a.stride=stride;a.offset=offset;
    if(m_vertexArrays.setAttribute(i,a,m_buffers)!=WKC::GLGX2::VertexArrayResult::Ok)addError(GL_INVALID_OPERATION_VALUE);
}
GCGLboolean GraphicsContextGLWKC::isBuffer(PlatformGLObject name)
{
    WKC::GLGX2::BufferInfo info;
    return name && m_buffers.info(name, info);
}

GCGLboolean GraphicsContextGLWKC::isProgram(PlatformGLObject name)
{
    return name && m_programs.contains(name);
}

GCGLboolean GraphicsContextGLWKC::isShader(PlatformGLObject name)
{
    // A delete-pending shader remains a valid object while programs still
    // reference it; deleting the shader only schedules its final destruction.
    return name && m_shaders.contains(name);
}

GCGLboolean GraphicsContextGLWKC::isEnabled(GCGLenum capability)
{
    switch (capability) {
    case GL_BLEND_VALUE: return m_native.blendEnabled;
    case GL_CULL_FACE_VALUE: return m_native.cullEnabled;
    case GL_DEPTH_TEST_VALUE: return m_native.depthTestEnabled;
    case GL_SCISSOR_TEST_VALUE: return m_native.scissorEnabled;
    case GL_STENCIL_TEST_VALUE: return m_native.stencilEnabled;
    case GL_POLYGON_OFFSET_FILL_VALUE: return m_native.polygonOffsetEnabled;
    default:
        addError(GL_INVALID_ENUM_VALUE);
        return false;
    }
}

GCGLsizeiptr GraphicsContextGLWKC::getVertexAttribOffset(GCGLuint index, GCGLenum pname)
{
    // GL_VERTEX_ATTRIB_ARRAY_POINTER is 0x8645. WebGL exposes only the pointer
    // value recorded by vertexAttribPointer; other pnames belong to getVertexAttrib.
    if (pname != 0x8645) {
        addError(GL_INVALID_ENUM_VALUE);
        return 0;
    }
    WKC::GLGX2::VertexAttribute attribute;
    if (index >= 32 || !m_vertexArrays.attribute(index, attribute)) {
        addError(GL_INVALID_VALUE_VALUE);
        return 0;
    }
    return static_cast<GCGLsizeiptr>(attribute.offset);
}

void GraphicsContextGLWKC::vertexAttribDivisor(GCGLuint i,GCGLuint divisor)
{
    WKC::GLGX2::VertexAttribute a;if(i>=32||!m_vertexArrays.attribute(i,a)){addError(GL_INVALID_VALUE_VALUE);return;}
    a.divisor=divisor;if(m_vertexArrays.setAttribute(i,a,m_buffers)!=WKC::GLGX2::VertexArrayResult::Ok)addError(GL_INVALID_OPERATION_VALUE);
}
PlatformGLObject GraphicsContextGLWKC::createVertexArray()
{
    WKC::GLGX2::VertexArrayName n=0;if(m_vertexArrays.create(n)!=WKC::GLGX2::VertexArrayResult::Ok){addError(GL_OUT_OF_MEMORY_VALUE);return 0;}
    m_vertexArrayObjects.emplace(n,true);return n;
}
void GraphicsContextGLWKC::deleteVertexArray(PlatformGLObject n){if(n&&m_vertexArrays.destroy(n)!=WKC::GLGX2::VertexArrayResult::Ok)addError(GL_INVALID_OPERATION_VALUE);m_vertexArrayObjects.erase(n);}
GCGLboolean GraphicsContextGLWKC::isVertexArray(PlatformGLObject n){return n&&m_vertexArrayObjects.contains(n);}
void GraphicsContextGLWKC::bindVertexArray(PlatformGLObject n){if(m_vertexArrays.bind(n)!=WKC::GLGX2::VertexArrayResult::Ok)addError(GL_INVALID_OPERATION_VALUE);}
void GraphicsContextGLWKC::scissor(GCGLint x,GCGLint y,GCGLsizei w,GCGLsizei h){addDriverError(m_driver.setScissor({x,y,w,h}),"scissor");}
void GraphicsContextGLWKC::polygonOffset(GCGLfloat f,GCGLfloat u){addDriverError(m_driver.setPolygonOffset(f,u),"polygonOffset");}
void GraphicsContextGLWKC::viewport(GCGLint x,GCGLint y,GCGLsizei w,GCGLsizei h)
{addDriverError(m_driver.setViewport({x,y,w,h,m_native.viewport.nearDepth,m_native.viewport.farDepth}),"viewport");}
void GraphicsContextGLWKC::bufferData(GCGLenum target,GCGLsizeiptr size,GCGLenum usage)
{
    WKC::GLGX2::BufferTarget t;if(!mapBufferTarget(target,t)){addError(GL_INVALID_ENUM_VALUE);return;}
    if(size<0){addError(GL_INVALID_VALUE_VALUE);return;}auto n=m_buffers.bound(t);if(!n){addError(GL_INVALID_OPERATION_VALUE);return;}
    if(m_buffers.allocate(n,size,mapBufferUsage(usage))!=WKC::GLGX2::BufferResult::Ok)addError(GL_OUT_OF_MEMORY_VALUE);
}
void GraphicsContextGLWKC::bufferData(GCGLenum target,std::span<const uint8_t> data,GCGLenum usage)
{
    WKC::GLGX2::BufferTarget t;if(!mapBufferTarget(target,t)){addError(GL_INVALID_ENUM_VALUE);return;}
    auto n=m_buffers.bound(t);if(!n){addError(GL_INVALID_OPERATION_VALUE);return;}
    if(m_buffers.allocate(n,data.size(),mapBufferUsage(usage),data.data())!=WKC::GLGX2::BufferResult::Ok)addError(GL_OUT_OF_MEMORY_VALUE);
}
void GraphicsContextGLWKC::bufferSubData(GCGLenum target,GCGLintptr offset,std::span<const uint8_t> data)
{
    WKC::GLGX2::BufferTarget t;if(!mapBufferTarget(target,t)){addError(GL_INVALID_ENUM_VALUE);return;}
    auto n=m_buffers.bound(t);if(!n||offset<0){addError(GL_INVALID_VALUE_VALUE);return;}
    if(m_buffers.update(n,offset,data.size(),data.data())!=WKC::GLGX2::BufferResult::Ok)addError(GL_INVALID_VALUE_VALUE);
}
void GraphicsContextGLWKC::reshape(int w,int h)
{
    if(w<0||h<0||!WKC::GLGX2::resizeWUTRenderTarget(m_native,w,h)){addError(GL_INVALID_VALUE_VALUE);return;}
    m_currentWidth=w;m_currentHeight=h;addDriverError(m_driver.setViewport({0,0,w,h,m_native.viewport.nearDepth,m_native.viewport.farDepth}),"reshape");
}
void GraphicsContextGLWKC::prepareForDisplay(){GX2DrawDone();WKC::GLGX2::bindWUTRenderTarget(m_native);}
GCGLint GraphicsContextGLWKC::maxCombinedTextureImageUnits(){return 16;}
GCGLint GraphicsContextGLWKC::maxVertexAttribs(){return 16;}
GCGLint GraphicsContextGLWKC::maxTextureSize(){return 4096;}
GCGLint GraphicsContextGLWKC::maxCubeMapTextureSize(){return 2048;}
GCGLint GraphicsContextGLWKC::maxRenderbufferSize(){return 4096;}
std::array<GCGLint,2> GraphicsContextGLWKC::maxViewportDims(){return {4096,4096};}
GCGLint GraphicsContextGLWKC::maxSamples(){return 1;}
GCGLint GraphicsContextGLWKC::maxTransformFeedbackSeparateAttribs(){return 0;}
GCGLint GraphicsContextGLWKC::maxUniformBufferBindings(){return 0;}
GCGLint GraphicsContextGLWKC::uniformBufferOffsetAlignment(){return 256;}
GCGLint GraphicsContextGLWKC::max3DTextureSize(){return 0;}
GCGLint GraphicsContextGLWKC::maxArrayTextureLayers(){return 0;}

#endif // __WUT__

RefPtr<GraphicsContextGL> createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes& attributes)
{
#if defined(__WUT__)
    if (attributes.isWebGL2)
        return nullptr;
    RefPtr context = adoptRef(*new GraphicsContextGLWKC(attributes));
    if (!context->initialize())
        return nullptr;
    return context;
#else
    UNUSED_PARAM(attributes);
    return nullptr;
#endif
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
