#include "OpenGLToGX2Shaders.h"

#include <cstring>
#include <limits>
#include <new>

namespace WKC {
namespace GLGX2 {

ShaderPrograms::ShaderPrograms(const ShaderBackend& backend)
    : m_backend(backend)
{
}

ShaderName ShaderPrograms::nextShaderName()
{
    for (uint64_t i = 0; i < std::numeric_limits<uint32_t>::max() - 1ULL; ++i) {
        ShaderName name = m_nextShader++;
        if (!m_nextShader)
            m_nextShader = 1;
        if (name && !m_shaders.count(name))
            return name;
    }
    return 0;
}

ProgramName ShaderPrograms::nextProgramName()
{
    for (uint64_t i = 0; i < std::numeric_limits<uint32_t>::max() - 1ULL; ++i) {
        ProgramName name = m_nextProgram++;
        if (!m_nextProgram)
            m_nextProgram = 1;
        if (name && !m_programs.count(name))
            return name;
    }
    return 0;
}

ShaderResult ShaderPrograms::createShader(ShaderStage stage, ShaderName& output)
{
    output = 0;
    if (stage != ShaderStage::Vertex && stage != ShaderStage::Fragment
        && stage != ShaderStage::Geometry && stage != ShaderStage::TessControl
        && stage != ShaderStage::TessEvaluation && stage != ShaderStage::Compute)
        return ShaderResult::InvalidArgument;
    ShaderName name = nextShaderName();
    if (!name)
        return ShaderResult::OutOfMemory;
    try {
        m_shaders.emplace(name, ShaderObject {});
        m_shaders[name].stage = stage;
    } catch (const std::bad_alloc&) {
        return ShaderResult::OutOfMemory;
    }
    output = name;
    return ShaderResult::Ok;
}

ShaderResult ShaderPrograms::setSource(ShaderName name, const char* source, size_t length)
{
    auto it = m_shaders.find(name);
    if (it == m_shaders.end())
        return ShaderResult::InvalidName;
    if (!source && length)
        return ShaderResult::InvalidArgument;
    try {
        it->second.source.assign(source ? source : "", length);
    } catch (const std::bad_alloc&) {
        return ShaderResult::OutOfMemory;
    }
    if (it->second.native && m_backend.destroyShader)
        m_backend.destroyShader(m_backend.context, it->second.native);
    it->second.native = 0;
    it->second.compiled = false;
    it->second.log.clear();
    return ShaderResult::Ok;
}

ShaderResult ShaderPrograms::compile(ShaderName name)
{
    auto it = m_shaders.find(name);
    if (it == m_shaders.end())
        return ShaderResult::InvalidName;
    ShaderObject& shader = it->second;
    shader.compiled = false;
    shader.log.clear();
    if (!m_backend.compile)
        return ShaderResult::MissingBackend;

    uint64_t native = 0;
    std::string log;
    if (!m_backend.compile(m_backend.context, shader.stage, shader.source.data(),
            shader.source.size(), native, log)) {
        shader.log = log;
        return ShaderResult::CompileFailure;
    }
    if (!native) {
        shader.log = "Backend reported successful compilation without a native shader handle.";
        return ShaderResult::BackendFailure;
    }
    if (shader.native && m_backend.destroyShader)
        m_backend.destroyShader(m_backend.context, shader.native);
    shader.native = native;
    shader.log = log;
    shader.compiled = true;
    return ShaderResult::Ok;
}

void ShaderPrograms::releaseShaderIfUnused(ShaderName name)
{
    auto it = m_shaders.find(name);
    if (it == m_shaders.end() || !it->second.deletePending)
        return;
    for (const auto& program : m_programs) {
        for (ShaderName attached : program.second.attached) {
            if (attached == name)
                return;
        }
    }
    if (it->second.native && m_backend.destroyShader)
        m_backend.destroyShader(m_backend.context, it->second.native);
    m_shaders.erase(it);
}

ShaderResult ShaderPrograms::deleteShader(ShaderName name)
{
    auto it = m_shaders.find(name);
    if (it == m_shaders.end())
        return ShaderResult::InvalidName;
    it->second.deletePending = true;
    releaseShaderIfUnused(name);
    return ShaderResult::Ok;
}

bool ShaderPrograms::shaderInfo(ShaderName name, ShaderInfo& output) const
{
    auto it = m_shaders.find(name);
    if (it == m_shaders.end())
        return false;
    output.stage = it->second.stage;
    output.compiled = it->second.compiled;
    output.log = it->second.log;
    return true;
}

ShaderResult ShaderPrograms::createProgram(ProgramName& output)
{
    output = 0;
    ProgramName name = nextProgramName();
    if (!name)
        return ShaderResult::OutOfMemory;
    try {
        m_programs.emplace(name, ProgramObject {});
    } catch (const std::bad_alloc&) {
        return ShaderResult::OutOfMemory;
    }
    output = name;
    return ShaderResult::Ok;
}

ShaderResult ShaderPrograms::attach(ProgramName program, ShaderName shader)
{
    auto p = m_programs.find(program);
    auto s = m_shaders.find(shader);
    if (p == m_programs.end() || s == m_shaders.end())
        return ShaderResult::InvalidName;
    if (p->second.linked)
        return ShaderResult::InvalidArgument;
    for (ShaderName attached : p->second.attached) {
        if (attached == shader)
            return ShaderResult::Ok;
    }
    try {
        p->second.attached.push_back(shader);
    } catch (const std::bad_alloc&) {
        return ShaderResult::OutOfMemory;
    }
    return ShaderResult::Ok;
}

ShaderResult ShaderPrograms::detach(ProgramName program, ShaderName shader)
{
    auto p = m_programs.find(program);
    if (p == m_programs.end())
        return ShaderResult::InvalidName;
    auto it = p->second.attached.begin();
    while (it != p->second.attached.end() && *it != shader)
        ++it;
    if (it == p->second.attached.end())
        return ShaderResult::InvalidName;
    p->second.attached.erase(it);
    releaseShaderIfUnused(shader);
    return ShaderResult::Ok;
}

ShaderResult ShaderPrograms::link(ProgramName name)
{
    auto it = m_programs.find(name);
    if (it == m_programs.end())
        return ShaderResult::InvalidName;
    ProgramObject& program = it->second;
    program.linked = false;
    program.log.clear();
    if (!m_backend.link)
        return ShaderResult::MissingBackend;
    if (program.attached.empty())
        return ShaderResult::LinkFailure;

    std::vector<uint64_t> nativeShaders;
    bool hasVertex = false;
    bool hasFragment = false;
    try {
        nativeShaders.reserve(program.attached.size());
        for (ShaderName nameAttached : program.attached) {
            auto shader = m_shaders.find(nameAttached);
            if (shader == m_shaders.end() || !shader->second.compiled || !shader->second.native) {
                program.log = "Every attached shader must compile successfully before linking.";
                return ShaderResult::LinkFailure;
            }
            hasVertex |= shader->second.stage == ShaderStage::Vertex;
            hasFragment |= shader->second.stage == ShaderStage::Fragment;
            nativeShaders.push_back(shader->second.native);
        }
    } catch (const std::bad_alloc&) {
        return ShaderResult::OutOfMemory;
    }
    if (!hasVertex || !hasFragment) {
        program.log = "A graphics program requires both vertex and fragment stages.";
        return ShaderResult::LinkFailure;
    }

    uint64_t nativeProgram = 0;
    std::string log;
    if (!m_backend.link(m_backend.context, nativeShaders.data(), nativeShaders.size(), nativeProgram, log)) {
        program.log = log;
        return ShaderResult::LinkFailure;
    }
    if (!nativeProgram) {
        program.log = "Backend reported successful linking without a native program handle.";
        return ShaderResult::BackendFailure;
    }
    if (program.native && m_backend.destroyProgram)
        m_backend.destroyProgram(m_backend.context, program.native);
    program.native = nativeProgram;
    program.log = log;
    program.linked = true;
    program.values.clear();
    return ShaderResult::Ok;
}

ShaderResult ShaderPrograms::use(ProgramName name)
{
    uint64_t native = 0;
    if (name) {
        auto it = m_programs.find(name);
        if (it == m_programs.end())
            return ShaderResult::InvalidName;
        if (!it->second.linked || !it->second.native)
            return ShaderResult::LinkFailure;
        native = it->second.native;
    }
    if (!m_backend.useProgram)
        return ShaderResult::MissingBackend;
    if (!m_backend.useProgram(m_backend.context, native))
        return ShaderResult::BackendFailure;

    // Uniform values set while this program was not current must be uploaded
    // when it becomes active, matching GL's per-program uniform storage.
    if (name) {
        const ProgramObject& program = m_programs.find(name)->second;
        if (!program.values.empty() && !m_backend.setUniform)
            return ShaderResult::MissingBackend;
        for (const auto& entry : program.values) {
            const UniformValue& value = entry.second;
            if (!m_backend.setUniform(m_backend.context, native, entry.first, value.type,
                    value.count, value.bytes.empty() ? nullptr : value.bytes.data(), value.bytes.size()))
                return ShaderResult::BackendFailure;
        }
    }
    m_current = name;
    return ShaderResult::Ok;
}

ShaderResult ShaderPrograms::deleteProgram(ProgramName name)
{
    auto it = m_programs.find(name);
    if (it == m_programs.end())
        return ShaderResult::InvalidName;
    if (m_current == name) {
        ShaderResult result = use(0);
        if (result != ShaderResult::Ok)
            return result;
    }
    std::vector<ShaderName> attached = it->second.attached;
    if (it->second.native && m_backend.destroyProgram)
        m_backend.destroyProgram(m_backend.context, it->second.native);
    m_programs.erase(it);
    for (ShaderName shader : attached)
        releaseShaderIfUnused(shader);
    return ShaderResult::Ok;
}

bool ShaderPrograms::programInfo(ProgramName name, ProgramInfo& output) const
{
    auto it = m_programs.find(name);
    if (it == m_programs.end())
        return false;
    output.linked = it->second.linked;
    output.log = it->second.log;
    return true;
}

size_t ShaderPrograms::uniformElementSize(UniformType type)
{
    switch (type) {
    case UniformType::Int:
    case UniformType::UInt:
    case UniformType::Float:
        return 4;
    case UniformType::Float2:
        return 8;
    case UniformType::Float3:
        return 12;
    case UniformType::Float4:
        return 16;
    case UniformType::Matrix4:
        return 64;
    }
    return 0;
}

ShaderResult ShaderPrograms::defineUniform(ProgramName name, const std::string& uniformName,
    UniformLocation location, UniformType type)
{
    auto it = m_programs.find(name);
    if (it == m_programs.end())
        return ShaderResult::InvalidName;
    if (!it->second.linked || location < 0 || uniformName.empty() || !uniformElementSize(type))
        return ShaderResult::InvalidArgument;
    try {
        it->second.definitions[location] = UniformDefinition { uniformName, type };
    } catch (const std::bad_alloc&) {
        return ShaderResult::OutOfMemory;
    }
    return ShaderResult::Ok;
}

ShaderResult ShaderPrograms::setUniform(ProgramName name, UniformLocation location,
    UniformType type, uint32_t count, const void* bytes, size_t byteLength)
{
    auto it = m_programs.find(name);
    if (it == m_programs.end())
        return ShaderResult::InvalidName;
    ProgramObject& program = it->second;
    if (!program.linked || location < 0)
        return ShaderResult::InvalidArgument;
    auto definition = program.definitions.find(location);
    if (definition == program.definitions.end())
        return ShaderResult::InvalidArgument;
    if (definition->second.type != type)
        return ShaderResult::InvalidArgument;
    const size_t elementSize = uniformElementSize(type);
    if (count && elementSize > std::numeric_limits<size_t>::max() / count)
        return ShaderResult::InvalidArgument;
    const size_t expected = elementSize * count;
    if (byteLength != expected || (expected && !bytes))
        return ShaderResult::InvalidArgument;

    if (m_current == name) {
        if (!m_backend.setUniform)
            return ShaderResult::MissingBackend;
        if (!m_backend.setUniform(m_backend.context, program.native, location, type, count, bytes, byteLength))
            return ShaderResult::BackendFailure;
    }
    try {
        UniformValue value;
        value.type = type;
        value.count = count;
        const uint8_t* begin = static_cast<const uint8_t*>(bytes);
        if (expected)
            value.bytes.assign(begin, begin + expected);
        program.values[location] = value;
    } catch (const std::bad_alloc&) {
        return ShaderResult::OutOfMemory;
    }
    return ShaderResult::Ok;
}

} // namespace GLGX2
} // namespace WKC
