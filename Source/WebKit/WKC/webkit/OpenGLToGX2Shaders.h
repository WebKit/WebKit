/*
 * OpenGL shader/program compatibility layer for the Wii U GX2 backend.
 *
 * A platform compiler callback must translate supported GLSL into the
 * GX2-supported shader representation. This class manages GL-style object
 * lifetimes and uniform values; it does not pretend GX2 executes desktop GLSL.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace WKC {
namespace GLGX2 {

using ShaderName = uint32_t;
using ProgramName = uint32_t;
using UniformLocation = int32_t;

enum class ShaderStage : uint8_t { Vertex, Fragment, Geometry, TessControl, TessEvaluation, Compute };
enum class ShaderResult : uint8_t {
    Ok, InvalidArgument, InvalidName, OutOfMemory, MissingBackend,
    CompileFailure, LinkFailure, BackendFailure, Unsupported
};
enum class UniformType : uint8_t { Int, UInt, Float, Float2, Float3, Float4, Matrix4 };

struct UniformValue {
    UniformType type { UniformType::Float };
    uint32_t count { 1 };
    std::vector<uint8_t> bytes;
};

struct ShaderBackend {
    void* context { nullptr };
    bool (*compile)(void*, ShaderStage, const char* source, size_t sourceLength,
        uint64_t& nativeShader, std::string& log) { nullptr };
    void (*destroyShader)(void*, uint64_t nativeShader) { nullptr };
    bool (*link)(void*, const uint64_t* nativeShaders, size_t shaderCount,
        uint64_t& nativeProgram, std::string& log) { nullptr };
    void (*destroyProgram)(void*, uint64_t nativeProgram) { nullptr };
    bool (*useProgram)(void*, uint64_t nativeProgram) { nullptr };
    bool (*setUniform)(void*, uint64_t nativeProgram, UniformLocation,
        UniformType, uint32_t count, const void* bytes, size_t byteLength) { nullptr };
};

struct ShaderInfo {
    ShaderStage stage { ShaderStage::Vertex };
    bool compiled { false };
    std::string log;
};

struct ProgramInfo {
    bool linked { false };
    std::string log;
};

class ShaderPrograms final {
public:
    explicit ShaderPrograms(const ShaderBackend&);
    ~ShaderPrograms();

    ShaderPrograms(const ShaderPrograms&) = delete;
    ShaderPrograms& operator=(const ShaderPrograms&) = delete;

    ShaderResult createShader(ShaderStage, ShaderName&);
    ShaderResult setSource(ShaderName, const char*, size_t);
    ShaderResult compile(ShaderName);
    ShaderResult deleteShader(ShaderName);
    bool shaderInfo(ShaderName, ShaderInfo&) const;

    ShaderResult createProgram(ProgramName&);
    ShaderResult attach(ProgramName, ShaderName);
    ShaderResult detach(ProgramName, ShaderName);
    ShaderResult link(ProgramName);
    ShaderResult use(ProgramName);
    ShaderResult deleteProgram(ProgramName);
    bool programInfo(ProgramName, ProgramInfo&) const;

    ShaderResult defineUniform(ProgramName, const std::string&, UniformLocation, UniformType);
    ShaderResult setUniform(ProgramName, UniformLocation, UniformType, uint32_t count,
        const void*, size_t byteLength);

private:
    struct ShaderObject {
        ShaderStage stage { ShaderStage::Vertex };
        std::string source;
        std::string log;
        uint64_t native { 0 };
        bool compiled { false };
        bool deletePending { false };
    };
    struct UniformDefinition {
        std::string name;
        UniformType type { UniformType::Float };
    };
    struct ProgramObject {
        std::vector<ShaderName> attached;
        std::unordered_map<UniformLocation, UniformDefinition> definitions;
        std::unordered_map<UniformLocation, UniformValue> values;
        std::string log;
        uint64_t native { 0 };
        bool linked { false };
        bool deletePending { false };
    };

    ShaderName nextShaderName();
    ProgramName nextProgramName();
    static size_t uniformElementSize(UniformType);
    void releaseShaderIfUnused(ShaderName);

    ShaderBackend m_backend;
    ShaderName m_nextShader { 1 };
    ProgramName m_nextProgram { 1 };
    ProgramName m_current { 0 };
    std::unordered_map<ShaderName, ShaderObject> m_shaders;
    std::unordered_map<ProgramName, ProgramObject> m_programs;
};

} // namespace GLGX2
} // namespace WKC
