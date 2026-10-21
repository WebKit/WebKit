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

    void bindTexture(GCGLenum target, PlatformGLObject) override { notImplemented(); }

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

    PlatformGLObject createTexture() override { notImplemented(); return {}; }

    void cullFace(GCGLenum mode) override;

    void deleteBuffer(PlatformGLObject) override;

    void deleteFramebuffer(PlatformGLObject) override { notImplemented(); }

    void deleteProgram(PlatformGLObject) override;

    void deleteRenderbuffer(PlatformGLObject) override { notImplemented(); }

    void deleteShader(PlatformGLObject) override;

    void deleteTexture(PlatformGLObject) override { notImplemented(); }

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

    void generateMipmap(GCGLenum target) override { notImplemented(); }

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

    GCGLfloat getTexParameterf(GCGLenum target, GCGLenum pname) override { notImplemented(); return {}; }

    GCGLint getTexParameteri(GCGLenum target, GCGLenum pname) override { notImplemented(); return {}; }

    void getUniformfv(PlatformGLObject program, GCGLint location, std::span<GCGLfloat> value) override { notImplemented(); }

    void getUniformiv(PlatformGLObject program, GCGLint location, std::span<GCGLint> value) override { notImplemented(); }

    void getUniformuiv(PlatformGLObject program, GCGLint location, std::span<GCGLuint> value) override { notImplemented(); }

    GCGLsizeiptr getVertexAttribOffset(GCGLuint index, GCGLenum pname) override { notImplemented(); return {}; }

    void hint(GCGLenum target, GCGLenum mode) override { notImplemented(); }

    GCGLboolean isBuffer(PlatformGLObject) override { notImplemented(); return {}; }

    GCGLboolean isEnabled(GCGLenum cap) override { notImplemented(); return {}; }

    GCGLboolean isFramebuffer(PlatformGLObject) override { notImplemented(); return {}; }

    GCGLboolean isProgram(PlatformGLObject) override { notImplemented(); return {}; }

    GCGLboolean isRenderbuffer(PlatformGLObject) override { notImplemented(); return {}; }

    GCGLboolean isShader(PlatformGLObject) override { notImplemented(); return {}; }

    GCGLboolean isTexture(PlatformGLObject) override { notImplemented(); return {}; }

    void lineWidth(GCGLfloat) override { notImplemented(); }

    void linkProgram(PlatformGLObject) override;

    void pixelStorei(GCGLenum pname, GCGLint param) override { notImplemented(); }

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

    void texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat param) override { notImplemented(); }

    void texParameteri(GCGLenum target, GCGLenum pname, GCGLint param) override { notImplemented(); }

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

    void texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type,  std::span<const uint8_t> pixels) override { notImplemented(); }

    void texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset) override { notImplemented(); }

    void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels) override { notImplemented(); }

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
    struct UniformBinding {
        GCGLint location { -1 };
        const GX2UniformVar* vertexVar { nullptr };
        const GX2UniformVar* pixelVar { nullptr };
        uint32_t arrayElement { 0 };
    };
    struct UniformRecord {
        std::string name;
        GCGLenum type { 0 };
        GCGLint size { 1 };
        std::vector<UniformBinding> elements;
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
    std::unordered_map<PlatformGLObject, bool> m_vertexArrayObjects;
    PlatformGLObject m_nextShader { 1 };
    PlatformGLObject m_nextProgram { 1 };
    PlatformGLObject m_currentProgram { 0 };
    PlatformGLObject m_appliedProgram { 0 };
    PlatformGLObject m_boundFramebuffer { 0 };
    GCGLuint m_activeTextureUnit { 0 };
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
}

bool GraphicsContextGLWKC::initialize()
{
    if (!m_compilerAvailable || !WKC::GLGX2::resizeWUTRenderTarget(m_native, 1, 1))
        return false;
    m_currentWidth = m_currentHeight = 1;
    addDriverError(m_driver.setViewport({ 0, 0, 1, 1, 0.0f, 1.0f }), "initial viewport");
    return true;
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
    if (m_appliedProgram==name && m_native.bufferResources.vertexShader==it->second.vertexShader) return true;
    std::vector<WKC::GLGX2::WUTLinkedAttribute> bindings;
    bindings.reserve(it->second.attributes.size());
    for (const auto& a:it->second.attributes)
        bindings.push_back({static_cast<uint8_t>(a.location),a.name.c_str()});
    if (!WKC::GLGX2::setWUTLinkedProgram(m_native.bufferResources,*it->second.vertexShader,*it->second.pixelShader,
            it->second.mode,bindings.data(),bindings.size())) {
        addError(GL_INVALID_OPERATION_VALUE); clearActiveNativeProgram(); return false;
    }
    m_currentProgram=name; m_appliedProgram=name;
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
    for(size_t i=0;i<p.uniforms.size();++i)
        for(size_t el=0;el<p.uniforms[i].elements.size();++el) {
            auto& b=p.uniforms[i].elements[el]; b.location=p.nextUniformLocation++;
            p.uniformLocations.emplace(b.location,UniformLocationInfo{i,el});
        }
}
bool GraphicsContextGLWKC::uploadUniformElement(const UniformBinding& b,GCGLenum type,const void* values,bool ints)
{
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
    const auto& uniform=p->second.uniforms[l->second.uniformIndex];
    if(matrixDim && components!=matrixDim*matrixDim) { addError(GL_INVALID_OPERATION_VALUE); return false; }
    if(bytes < static_cast<size_t>(components)*count*(ints?sizeof(int32_t):sizeof(float))) { addError(GL_INVALID_VALUE_VALUE); return false; }
    const auto* data=static_cast<const uint8_t*>(values);
    for(uint32_t i=0;i<count;++i) {
        const size_t element=l->second.elementIndex+i;
        if(element>=uniform.elements.size()) break;
        if(!uploadUniformElement(uniform.elements[element],uniform.type,data+static_cast<size_t>(i)*components*(ints?sizeof(int32_t):sizeof(float)),ints)) {
            addError(GL_INVALID_OPERATION_VALUE); return false;
        }
    }
    return true;
}
RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLWKC::layerContentsDisplayDelegate() { return nullptr; }

void GraphicsContextGLWKC::activeTexture(GCGLenum tex)
{
    if(tex<0x84C0 || tex>=0x84C0+16) { addError(GL_INVALID_ENUM_VALUE); return; }
    m_activeTextureUnit=tex-0x84C0;
}
void GraphicsContextGLWKC::attachShader(PlatformGLObject p,PlatformGLObject s)
{
    auto pi=m_programs.find(p);
    if(pi==m_programs.end() || !m_shaders.contains(s)) { addError(GL_INVALID_VALUE_VALUE); return; }
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
    auto it=m_programs.find(name);if(it==m_programs.end())return;
    if(m_currentProgram==name){m_currentProgram=0;clearActiveNativeProgram();}
    releaseProgramNative(it->second);m_programs.erase(it);
}
void GraphicsContextGLWKC::deleteShader(PlatformGLObject name)
{
    auto it=m_shaders.find(name);if(it==m_shaders.end())return;
    it->second.deletePending=true;
    FreeVertexShader(it->second.compiledVertex);FreePixelShader(it->second.compiledPixel);
    it->second.compiledVertex=nullptr;it->second.compiledPixel=nullptr;
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
    auto& a=i->second.shaders;a.erase(std::remove(a.begin(),a.end(),s),a.end());
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
GCGLErrorCodeSet GraphicsContextGLWKC::getErrors(){auto e=m_errors;m_errors={};return e;}
void GraphicsContextGLWKC::linkProgram(PlatformGLObject n)
{
    auto it=m_programs.find(n);if(it==m_programs.end()){addError(GL_INVALID_VALUE_VALUE);return;}
    auto& p=it->second;p.log.clear();p.linked=false;p.validated=false;releaseProgramNative(p);
    ShaderRecord* vs=nullptr;ShaderRecord* ps=nullptr;
    for(auto sname:p.shaders){auto s=m_shaders.find(sname);if(s==m_shaders.end()||!s->second.compiled||s->second.deletePending){p.log="Attached shaders must compile and remain attached.";return;}
        if(s->second.type==GL_VERTEX_SHADER_VALUE)vs=&s->second;if(s->second.type==GL_FRAGMENT_SHADER_VALUE)ps=&s->second;}
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
