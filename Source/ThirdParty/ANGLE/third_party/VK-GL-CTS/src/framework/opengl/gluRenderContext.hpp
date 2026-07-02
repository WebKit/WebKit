#ifndef _GLURENDERCONTEXT_HPP
#define _GLURENDERCONTEXT_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief OpenGL ES rendering context.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

// glw::GenericFuncType
#include "glwFunctionLoader.hpp"

namespace tcu
{
class CommandLine;
class Platform;
class RenderTarget;
} // namespace tcu

namespace glw
{
class Functions;
class FunctionLoader;
} // namespace glw

namespace glu
{

class ContextType;
class ContextInfo;
struct RenderConfig;

enum Profile
{
    PROFILE_ES = 0,        //!< OpenGL ES
    PROFILE_CORE,          //!< OpenGL Core Profile
    PROFILE_COMPATIBILITY, //!< OpenGL Compatibility Profile

    PROFILE_LAST
};

enum ContextFlags
{
    CONTEXT_ROBUST             = (1 << 0), //!< Robust context
    CONTEXT_DEBUG              = (1 << 1), //!< Debug context
    CONTEXT_FORWARD_COMPATIBLE = (1 << 2), //!< Forward-compatible context
    CONTEXT_NO_ERROR           = (1 << 3)  //!< No error context
};

inline ContextFlags operator|(ContextFlags a, ContextFlags b)
{
    return ContextFlags((uint32_t)a | (uint32_t)b);
}
inline ContextFlags operator&(ContextFlags a, ContextFlags b)
{
    return ContextFlags((uint32_t)a & (uint32_t)b);
}
inline ContextFlags operator~(ContextFlags a)
{
    return ContextFlags(~(uint32_t)a);
}

/*--------------------------------------------------------------------*//*!
 * \brief Rendering API version and profile.
 *//*--------------------------------------------------------------------*/
class ApiType
{
public:
    ApiType(void) : m_bits(pack(0, 0, PROFILE_LAST))
    {
    }
    ApiType(int major, int minor, Profile profile) : m_bits(pack(major, minor, profile))
    {
    }

    int getMajorVersion(void) const
    {
        return int((m_bits >> MAJOR_SHIFT) & ((1u << MAJOR_BITS) - 1u));
    }
    int getMinorVersion(void) const
    {
        return int((m_bits >> MINOR_SHIFT) & ((1u << MINOR_BITS) - 1u));
    }
    Profile getProfile(void) const
    {
        return Profile((m_bits >> PROFILE_SHIFT) & ((1u << PROFILE_BITS) - 1u));
    }

    bool operator==(ApiType other) const
    {
        return m_bits == other.m_bits;
    }
    bool operator!=(ApiType other) const
    {
        return m_bits != other.m_bits;
    }

    uint32_t getPacked(void) const
    {
        return m_bits;
    }

    // Shorthands
    static ApiType es(int major, int minor)
    {
        return ApiType(major, minor, PROFILE_ES);
    }
    static ApiType core(int major, int minor)
    {
        return ApiType(major, minor, PROFILE_CORE);
    }
    static ApiType compatibility(int major, int minor)
    {
        return ApiType(major, minor, PROFILE_COMPATIBILITY);
    }

protected:
    ApiType(uint32_t bits) : m_bits(bits)
    {
    }
    static ApiType fromBits(uint32_t bits)
    {
        return ApiType(bits);
    }

    static uint32_t pack(int major, int minor, Profile profile);

    uint32_t m_bits;

    enum
    {
        MAJOR_BITS     = 4,
        MINOR_BITS     = 4,
        PROFILE_BITS   = 2,
        TOTAL_API_BITS = MAJOR_BITS + MINOR_BITS + PROFILE_BITS,

        MAJOR_SHIFT   = 0,
        MINOR_SHIFT   = MAJOR_SHIFT + MAJOR_BITS,
        PROFILE_SHIFT = MINOR_SHIFT + MINOR_BITS
    };
} DE_WARN_UNUSED_TYPE;

inline uint32_t ApiType::pack(int major, int minor, Profile profile)
{
    uint32_t bits = 0;

    DE_ASSERT((uint32_t(major) & ~((1 << MAJOR_BITS) - 1)) == 0);
    DE_ASSERT((uint32_t(minor) & ~((1 << MINOR_BITS) - 1)) == 0);
    DE_ASSERT((uint32_t(profile) & ~((1 << PROFILE_BITS) - 1)) == 0);

    bits |= uint32_t(major) << MAJOR_SHIFT;
    bits |= uint32_t(minor) << MINOR_SHIFT;
    bits |= uint32_t(profile) << PROFILE_SHIFT;

    return bits;
}

/*--------------------------------------------------------------------*//*!
 * \brief Rendering context type.
 *
 * ContextType differs from API type by adding context flags. They are
 * crucial in for example determining when GL core context supports
 * certain API version (forward-compatible bit).
 *
 * \note You should NEVER compare ContextTypes against each other, as
 *       you most likely don't want to take flags into account. For example
 *       the test code almost certainly doesn't want to check that you have
 *       EXACTLY ES3.1 context with debug, but without for example robustness.
 *//*--------------------------------------------------------------------*/
class ContextType : private ApiType
{
public:
    ContextType(void)
    {
    }
    ContextType(int major, int minor, Profile profile, ContextFlags flags = ContextFlags(0));
    explicit ContextType(ApiType apiType, ContextFlags flags = ContextFlags(0));

    ApiType getAPI(void) const
    {
        return ApiType::fromBits(m_bits & ((1u << TOTAL_API_BITS) - 1u));
    }
    void setAPI(const ApiType &apiType)
    {
        m_bits = apiType.getPacked();
    }

    ContextFlags getFlags(void) const
    {
        return ContextFlags((m_bits >> FLAGS_SHIFT) & ((1u << FLAGS_BITS) - 1u));
    }

    using ApiType::getMajorVersion;
    using ApiType::getMinorVersion;
    using ApiType::getProfile;

protected:
    static uint32_t pack(uint32_t apiBits, ContextFlags flags);

    enum
    {
        FLAGS_BITS         = 4,
        TOTAL_CONTEXT_BITS = TOTAL_API_BITS + FLAGS_BITS,
        FLAGS_SHIFT        = TOTAL_API_BITS
    };
} DE_WARN_UNUSED_TYPE;

inline ContextType::ContextType(int major, int minor, Profile profile, ContextFlags flags)
    : ApiType(major, minor, profile)
{
    m_bits = pack(m_bits, flags);
}

inline ContextType::ContextType(ApiType apiType, ContextFlags flags) : ApiType(apiType)
{
    m_bits = pack(m_bits, flags);
}

inline uint32_t ContextType::pack(uint32_t apiBits, ContextFlags flags)
{
    uint32_t bits = apiBits;

    DE_ASSERT((uint32_t(flags) & ~((1u << FLAGS_BITS) - 1u)) == 0);

    bits |= uint32_t(flags) << FLAGS_SHIFT;

    return bits;
}

inline bool isContextTypeES(ContextType type)
{
    return type.getAPI().getProfile() == PROFILE_ES;
}
inline bool isContextTypeGLCore(ContextType type)
{
    return type.getAPI().getProfile() == PROFILE_CORE;
}
inline bool isContextTypeGLCompatibility(ContextType type)
{
    return type.getAPI().getProfile() == PROFILE_COMPATIBILITY;
}
inline bool isES2Context(ContextType type)
{
    return isContextTypeES(type) && type.getMajorVersion() == 2;
}
bool contextSupports(ContextType ctxType, ApiType requiredApiType);

const char *getApiTypeDescription(ApiType type);

/*--------------------------------------------------------------------*//*!
 * \brief Rendering context abstraction.
 *//*--------------------------------------------------------------------*/
class RenderContext
{
public:
    RenderContext(void)
    {
    }
    virtual ~RenderContext(void)
    {
    }

    //! Get context type. Must match to type given to ContextFactory::createContext().
    virtual ContextType getType(void) const = 0;

    //! Get GL function table. Should be filled with all core entry points for context type.
    virtual const glw::Functions &getFunctions(void) const = 0;

    //! Get render target information.
    virtual const tcu::RenderTarget &getRenderTarget(void) const = 0;

    //! Do post-render actions (swap buffers for example).
    virtual void postIterate(void) = 0;

    //! Get default framebuffer.
    virtual uint32_t getDefaultFramebuffer(void) const
    {
        return 0;
    }

    //! Get extension function address.
    virtual glw::GenericFuncType getProcAddress(const char *name) const;

    //! Make context current in thread. Optional to support.
    virtual void makeCurrent(void);

private:
    RenderContext(const RenderContext &other);            // Not allowed!
    RenderContext &operator=(const RenderContext &other); // Not allowed!
};

// Utilities

RenderContext *createRenderContext(tcu::Platform &platform, const tcu::CommandLine &cmdLine, const RenderConfig &config,
                                   const RenderContext *sharedContext = nullptr);
RenderContext *createDefaultRenderContext(tcu::Platform &platform, const tcu::CommandLine &cmdLine, ApiType apiType);

void initCoreFunctions(glw::Functions *dst, const glw::FunctionLoader *loader, ApiType apiType);
void initExtensionFunctions(glw::Functions *dst, const glw::FunctionLoader *loader, ApiType apiType, int numExtensions,
                            const char *const *extensions);

// \note initFunctions() and initExtensionFunctions() without explicit extension list
//         use glGetString* to query list of extensions, so it needs current GL context.
void initFunctions(glw::Functions *dst, const glw::FunctionLoader *loader, ApiType apiType);
void initExtensionFunctions(glw::Functions *dst, const glw::FunctionLoader *loader, ApiType apiType);

bool hasExtension(const glw::Functions &gl, ApiType apiType, const std::string &extension);

} // namespace glu

#endif // _GLURENDERCONTEXT_HPP
