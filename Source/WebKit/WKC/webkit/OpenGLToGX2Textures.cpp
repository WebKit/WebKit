#include "OpenGLToGX2Textures.h"

#include <limits>
#include <new>

namespace WKC {
namespace GLGX2 {
namespace {

uint32_t bytesPerPixel(TextureFormat format)
{
    switch (format) {
    case TextureFormat::R8: return 1;
    case TextureFormat::RG8: return 2;
    case TextureFormat::RGB8: return 3;
    case TextureFormat::RGBA8:
    case TextureFormat::SRGB8Alpha8:
    case TextureFormat::R32F:
    case TextureFormat::Depth24:
    case TextureFormat::Depth24Stencil8: return 4;
    case TextureFormat::R16F:
    case TextureFormat::Depth16: return 2;
    case TextureFormat::RG16F: return 4;
    case TextureFormat::RGBA16F: return 8;
    case TextureFormat::RG32F: return 8;
    case TextureFormat::RGBA32F: return 16;
    }
    return 0;
}

bool requiredBytes(TextureFormat format, uint32_t width, uint32_t height, size_t& result)
{
    const uint32_t bpp = bytesPerPixel(format);
    if (!bpp || !width || !height)
        return false;
    const uint64_t bytes = static_cast<uint64_t>(width) * height * bpp;
    if (bytes > std::numeric_limits<size_t>::max())
        return false;
    result = static_cast<size_t>(bytes);
    return true;
}

} // namespace

Textures::Textures(const TextureBackend& backend)
    : m_backend(backend)
{
}

bool Textures::validTarget(TextureTarget target)
{
    switch (target) {
    case TextureTarget::Texture2D:
    case TextureTarget::CubeMap:
    case TextureTarget::Texture3D:
    case TextureTarget::Texture2DArray:
        return true;
    }
    return false;
}

bool Textures::validDescriptor(const TextureDescriptor& descriptor)
{
    if (!validTarget(descriptor.target) || !descriptor.width || !descriptor.height
        || !descriptor.depth || !descriptor.levels || !bytesPerPixel(descriptor.format))
        return false;

    if ((descriptor.target == TextureTarget::Texture2D
            && descriptor.depth != 1)
        || (descriptor.target == TextureTarget::CubeMap
            && (descriptor.depth != 6 || descriptor.width != descriptor.height)))
        return false;

    uint32_t largest = descriptor.width > descriptor.height ? descriptor.width : descriptor.height;
    if (descriptor.target == TextureTarget::Texture3D && descriptor.depth > largest)
        largest = descriptor.depth;
    uint32_t maxLevels = 1;
    while (largest > 1) {
        largest >>= 1;
        ++maxLevels;
    }
    return descriptor.levels <= maxLevels;
}

bool Textures::levelDimensions(const TextureDescriptor& descriptor, uint32_t level,
    uint32_t& width, uint32_t& height, uint32_t& depth)
{
    if (level >= descriptor.levels)
        return false;
    width = descriptor.width;
    height = descriptor.height;
    depth = descriptor.depth;
    for (uint32_t i = 0; i < level; ++i) {
        if (width > 1) width >>= 1;
        if (height > 1) height >>= 1;
        if (descriptor.target == TextureTarget::Texture3D && depth > 1)
            depth >>= 1;
    }
    return true;
}

TextureName Textures::nextName()
{
    for (uint64_t attempts = 0; attempts < std::numeric_limits<TextureName>::max(); ++attempts) {
        TextureName candidate = m_nextName++;
        if (!m_nextName)
            m_nextName = 1;
        if (candidate && !m_objects.count(candidate))
            return candidate;
    }
    return 0;
}

TextureResult Textures::create(TextureTarget target, TextureName& output)
{
    output = 0;
    if (!validTarget(target))
        return TextureResult::InvalidArgument;
    if (!m_backend.create || !m_backend.destroy)
        return TextureResult::MissingBackend;

    const TextureName name = nextName();
    if (!name)
        return TextureResult::OutOfMemory;

    Object object;
    object.target = target;
    object.storage.target = target;
    if (!m_backend.create(m_backend.context, target, object.native) || !object.native)
        return TextureResult::BackendFailure;

    try {
        m_objects.emplace(name, object);
    } catch (const std::bad_alloc&) {
        m_backend.destroy(m_backend.context, object.native);
        return TextureResult::OutOfMemory;
    }

    output = name;
    return TextureResult::Ok;
}

TextureResult Textures::destroy(TextureName name)
{
    auto it = m_objects.find(name);
    if (it == m_objects.end())
        return TextureResult::InvalidName;
    if (!m_backend.destroy)
        return TextureResult::MissingBackend;

    m_backend.destroy(m_backend.context, it->second.native);
    m_objects.erase(it);
    return TextureResult::Ok;
}

TextureResult Textures::allocate(TextureName name, const TextureDescriptor& descriptor)
{
    auto it = m_objects.find(name);
    if (it == m_objects.end())
        return TextureResult::InvalidName;
    if (!validDescriptor(descriptor) || descriptor.target != it->second.target)
        return TextureResult::InvalidArgument;
    if (!m_backend.allocate)
        return TextureResult::MissingBackend;
    if (!m_backend.allocate(m_backend.context, it->second.native, descriptor))
        return TextureResult::Unsupported;

    it->second.storage = descriptor;
    it->second.allocated = true;
    return TextureResult::Ok;
}

TextureResult Textures::upload(TextureName name, uint32_t level, uint32_t layer,
    uint32_t width, uint32_t height, const void* pixels, size_t byteLength)
{
    auto it = m_objects.find(name);
    if (it == m_objects.end())
        return TextureResult::InvalidName;
    const Object& object = it->second;
    if (!object.allocated)
        return TextureResult::InvalidArgument;

    uint32_t levelWidth, levelHeight, levelDepth;
    if (!levelDimensions(object.storage, level, levelWidth, levelHeight, levelDepth)
        || !width || !height || width > levelWidth || height > levelHeight
        || layer >= levelDepth || (!pixels && byteLength))
        return TextureResult::InvalidArgument;

    size_t minimumBytes = 0;
    if (!requiredBytes(object.storage.format, width, height, minimumBytes)
        || byteLength < minimumBytes || !pixels)
        return TextureResult::InvalidArgument;
    if (!m_backend.upload)
        return TextureResult::MissingBackend;
    if (!m_backend.upload(m_backend.context, object.native, level, layer,
            object.storage.format, width, height, pixels, byteLength))
        return TextureResult::Unsupported;
    return TextureResult::Ok;
}

TextureResult Textures::bind(uint32_t unit, TextureName name)
{
    auto it = m_objects.find(name);
    if (it == m_objects.end())
        return TextureResult::InvalidName;
    if (!m_backend.bind)
        return TextureResult::MissingBackend;
    if (!m_backend.bind(m_backend.context, unit, it->second.target, it->second.native))
        return TextureResult::Unsupported;
    return TextureResult::Ok;
}

TextureResult Textures::setSampler(TextureName name, uint32_t minFilter, uint32_t magFilter,
    uint32_t wrapS, uint32_t wrapT, uint32_t wrapR)
{
    auto it = m_objects.find(name);
    if (it == m_objects.end())
        return TextureResult::InvalidName;
    if (!m_backend.setSampler)
        return TextureResult::MissingBackend;
    if (!m_backend.setSampler(m_backend.context, it->second.native,
            minFilter, magFilter, wrapS, wrapT, wrapR))
        return TextureResult::Unsupported;
    return TextureResult::Ok;
}

bool Textures::info(TextureName name, TextureInfo& output) const
{
    auto it = m_objects.find(name);
    if (it == m_objects.end())
        return false;
    output.target = it->second.target;
    output.storage = it->second.storage;
    output.allocated = it->second.allocated;
    return true;
}

} // namespace GLGX2
} // namespace WKC
