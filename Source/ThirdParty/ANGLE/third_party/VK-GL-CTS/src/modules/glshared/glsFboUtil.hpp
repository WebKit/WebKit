#ifndef _GLSFBOUTIL_HPP
#define _GLSFBOUTIL_HPP

/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Utilities for framebuffer objects.
 *//*--------------------------------------------------------------------*/

#include "gluRenderContext.hpp"
#include "gluContextInfo.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "gluTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuDefs.hpp"

#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <iterator>

namespace deqp
{
namespace gls
{

//! A pair of iterators to present a range.
//! \note This must be POD to allow static initialization.
//! \todo [2013-12-03 lauri] Move this to decpp?
template <typename T>
struct Range
{
    typedef const T *const_iterator;

    const T *m_begin;
    const T *m_end;

    const T *begin(void) const
    {
        return m_begin;
    }
    const T *end(void) const
    {
        return m_end;
    }
};

#define GLS_ARRAY_RANGE(ARR)                   \
    {                                          \
        DE_ARRAY_BEGIN(ARR), DE_ARRAY_END(ARR) \
    }

#define GLS_NULL_RANGE   \
    {                    \
        nullptr, nullptr \
    }

//! A pair type that, unlike stl::pair, is POD so it can be statically initialized.
template <typename T1, typename T2>
struct Pair
{
    typedef T1 first_type;
    typedef T2 second_type;
    T1 first;
    T2 second;
};

namespace FboUtil
{

//! Configurations for framebuffer objects and their attachments.

class FboVerifier;
class FboBuilder;

typedef uint32_t FormatKey;

#define GLS_UNSIZED_FORMATKEY(FORMAT, TYPE) (uint32_t(TYPE) << 16 | uint32_t(FORMAT))

typedef Range<FormatKey> FormatKeys;

struct ImageFormat
{
    glw::GLenum format;

    //! Type if format is unsized, GL_NONE if sized.
    glw::GLenum unsizedType;

    bool operator<(const ImageFormat &other) const
    {
        return (format < other.format || (format == other.format && unsizedType < other.unsizedType));
    }

    static ImageFormat none(void)
    {
        ImageFormat fmt = {GL_NONE, GL_NONE};
        return fmt;
    }
};

std::ostream &operator<<(std::ostream &stream, const ImageFormat &format);

static inline ImageFormat formatKeyInfo(FormatKey key)
{
    ImageFormat fmt = {key & 0xffff, key >> 16};
    return fmt;
}

enum FormatFlags
{
    ANY_FORMAT          = 0,
    COLOR_RENDERABLE    = 1 << 0,
    DEPTH_RENDERABLE    = 1 << 1,
    STENCIL_RENDERABLE  = 1 << 2,
    RENDERBUFFER_VALID  = 1 << 3,
    TEXTURE_VALID       = 1 << 4,
    REQUIRED_RENDERABLE = 1 << 5, //< Without this, renderability is allowed, not required.
};

static inline FormatFlags operator|(FormatFlags f1, FormatFlags f2)
{
    return FormatFlags(uint32_t(f1) | uint32_t(f2));
}

FormatFlags formatFlag(glw::GLenum context);

typedef std::set<ImageFormat> Formats;

class FormatDB
{
public:
    void addCoreFormat(ImageFormat format, FormatFlags flags);
    void addExtensionFormat(ImageFormat format, FormatFlags flags, const std::set<std::string> &requiredExtensions);

    Formats getFormats(FormatFlags requirements) const;
    bool isKnownFormat(ImageFormat format) const;
    FormatFlags getFormatInfo(ImageFormat format) const;
    std::set<std::set<std::string>> getFormatFeatureExtensions(ImageFormat format, FormatFlags requirements) const;

private:
    struct ExtensionInfo
    {
        FormatFlags flags;
        std::set<std::string> requiredExtensions;

        bool operator<(const ExtensionInfo &other) const;
    };

    typedef std::map<ImageFormat, FormatFlags> FormatMap;
    typedef std::map<ImageFormat, std::set<ExtensionInfo>> FormatExtensionMap;

    FormatMap m_formatFlags;
    FormatExtensionMap m_formatExtensions;
};

typedef Pair<FormatFlags, FormatKeys> FormatEntry;
typedef Range<FormatEntry> FormatEntries;

// \todo [2013-12-20 lauri] It turns out that format properties in extensions
// are actually far too fine-grained for this bundling to be reasonable,
// especially given the syntactic cumbersomeness of static arrays. It's better
// to list each entry separately.

struct FormatExtEntry
{
    const char *extensions;
    uint32_t flags;
    Range<FormatKey> formats;
};

typedef Range<FormatExtEntry> FormatExtEntries;

// Check support for GL_* and DEQP_* extensions
bool checkExtensionSupport(const glu::RenderContext &ctx, const std::string &extension);

// Accepts GL_* and DEQP_* extension strings and converts DEQP_* strings to a human readable string
std::string getExtensionDescription(const std::string &extensionName);

void addFormats(FormatDB &db, FormatEntries stdFmts);
void addExtFormats(FormatDB &db, FormatExtEntries extFmts, const glu::RenderContext *ctx);
glu::TransferFormat transferImageFormat(const ImageFormat &imgFormat);

namespace config
{

struct Config
{
    virtual ~Config(void)
    {
    }
};

struct Image : public Config
{
    ImageFormat internalFormat;
    glw::GLsizei width;
    glw::GLsizei height;

protected:
    Image(void) : internalFormat(ImageFormat::none()), width(0), height(0)
    {
    }
};

struct Renderbuffer : public Image
{
    Renderbuffer(void) : numSamples(0)
    {
    }

    glw::GLsizei numSamples;
};

struct Texture : public Image
{
    Texture(void) : numLevels(1)
    {
    }

    glw::GLint numLevels;
};

struct TextureFlat : public Texture
{
};

struct Texture2D : public TextureFlat
{
};

struct TextureCubeMap : public TextureFlat
{
};

struct TextureLayered : public Texture
{
    TextureLayered(void) : numLayers(1)
    {
    }
    glw::GLsizei numLayers;
};

struct Texture3D : public TextureLayered
{
};

struct Texture2DArray : public TextureLayered
{
};

struct Attachment : public Config
{
    Attachment(void) : target(GL_FRAMEBUFFER), imageName(0)
    {
    }

    glw::GLenum target;
    glw::GLuint imageName;

    //! Returns `true` iff this attachment is "framebuffer attachment
    //! complete" when bound to attachment point `attPoint`, and the current
    //! image with name `imageName` is `image`, using `vfr` to check format
    //! renderability.
    bool isComplete(glw::GLenum attPoint, const Image *image, const FboVerifier &vfr) const;
};

struct RenderbufferAttachment : public Attachment
{
    RenderbufferAttachment(void) : renderbufferTarget(GL_RENDERBUFFER)
    {
    }

    glw::GLenum renderbufferTarget;
};

struct TextureAttachment : public Attachment
{
    TextureAttachment(void) : level(0)
    {
    }

    glw::GLint level;
};

struct TextureFlatAttachment : public TextureAttachment
{
    TextureFlatAttachment(void) : texTarget(GL_NONE)
    {
    }

    glw::GLenum texTarget;
};

struct TextureLayerAttachment : public TextureAttachment
{
    TextureLayerAttachment(void) : layer(0)
    {
    }

    glw::GLsizei layer;
};

glw::GLenum attachmentType(const Attachment &att);
glw::GLsizei imageNumSamples(const Image &img);

//! Mapping from attachment points to attachment configurations.
typedef std::map<glw::GLenum, const Attachment *> AttachmentMap;

//! Mapping from object names to texture configurations.
typedef std::map<glw::GLuint, const Texture *> TextureMap;

//! Mapping from object names to renderbuffer configurations.
typedef std::map<glw::GLuint, const Renderbuffer *> RboMap;

//! A framebuffer configuration.
struct Framebuffer
{
    AttachmentMap attachments;
    TextureMap textures;
    RboMap rbos;

    void attach(glw::GLenum attPoint, const Attachment *att);
    void setTexture(glw::GLuint texName, const Texture &texCfg);
    void setRbo(glw::GLuint rbName, const Renderbuffer &rbCfg);
    const Image *getImage(glw::GLenum type, glw::GLuint imgName) const;
};

} // namespace config

class FboBuilder : public config::Framebuffer
{
public:
    void glAttach(glw::GLenum attPoint, const config::Attachment *att);
    glw::GLuint glCreateTexture(const config::Texture &texCfg);
    glw::GLuint glCreateRbo(const config::Renderbuffer &rbCfg);
    FboBuilder(glw::GLuint fbo, glw::GLenum target, const glw::Functions &gl);
    ~FboBuilder(void);
    glw::GLenum getError(void)
    {
        return m_error;
    }

    //! Allocate a new configuration of type `Config` (which must be a
    //! subclass of `config::Config`), and return a referenc to it. The newly
    //! allocated object will be freed when this builder object is destroyed.
    template <typename Config>
    Config &makeConfig(void)
    {
        Config *cfg = new Config();
        m_configs.insert(cfg);
        return *cfg;
    }

private:
    typedef std::set<config::Config *> Configs;

    void checkError(void);

    glw::GLenum m_error; //< The first GL error encountered.
    glw::GLenum m_target;
    const glw::Functions &m_gl;
    Configs m_configs;
};

struct ValidStatusCodes
{
    ValidStatusCodes(void);

    bool isFBOStatusValid(glw::GLenum fboStatus) const;
    bool isFBOStatusRequired(glw::GLenum fboStatus) const;
    bool isErrorCodeValid(glw::GLenum errorCode) const;
    bool isErrorCodeRequired(glw::GLenum errorCode) const;

    void addErrorCode(glw::GLenum error, const char *description);
    void addFBOErrorStatus(glw::GLenum status, const char *description);
    void setAllowComplete(bool);

    void logLegalResults(tcu::TestLog &log) const;
    void logRules(tcu::TestLog &log) const;

private:
    struct RuleViolation
    {
        glw::GLenum errorCode;
        std::set<std::string> rules;
    };

    void logRule(tcu::TestLog &log, const std::string &ruleName, const std::set<std::string> &rules) const;
    void addViolation(std::vector<RuleViolation> &dst, glw::GLenum code, const char *description) const;

    std::vector<RuleViolation> m_errorCodes;    //!< Allowed GL errors, GL_NO_ERROR is not allowed
    std::vector<RuleViolation> m_errorStatuses; //!< Allowed FBO error statuses, GL_FRAMEBUFFER_COMPLETE is not allowed
    bool m_allowComplete;                       //!< true if (GL_NO_ERROR && GL_FRAMEBUFFER_COMPLETE) is allowed
};

void logFramebufferConfig(const config::Framebuffer &cfg, tcu::TestLog &log);

class Checker
{
public:
    Checker(const glu::RenderContext &, const FormatDB &);
    virtual ~Checker(void)
    {
    }

    void addGLError(glw::GLenum error, const char *description);
    void addPotentialGLError(glw::GLenum error, const char *description);
    void addFBOStatus(glw::GLenum status, const char *description);
    void addPotentialFBOStatus(glw::GLenum status, const char *description);

    ValidStatusCodes getStatusCodes(void)
    {
        return m_statusCodes;
    }

    virtual void check(glw::GLenum attPoint, const config::Attachment &att, const config::Image *image) = 0;

protected:
    const glu::RenderContext &m_renderCtx;
    const FormatDB &m_formats;

private:
    ValidStatusCodes m_statusCodes; //< Allowed return values for glCheckFramebufferStatus.
};

class CheckerFactory
{
public:
    virtual Checker *createChecker(const glu::RenderContext &, const FormatDB &) = 0;
};

typedef std::set<glw::GLenum> AttachmentPoints;
typedef std::set<ImageFormat> Formats;

class FboVerifier
{
public:
    FboVerifier(const FormatDB &formats, CheckerFactory &factory, const glu::RenderContext &renderCtx);

    ValidStatusCodes validStatusCodes(const config::Framebuffer &cfg) const;

private:
    const FormatDB &m_formats;
    CheckerFactory &m_factory;
    const glu::RenderContext &m_renderCtx;
};

} // namespace FboUtil
} // namespace gls
} // namespace deqp

#endif // _GLSFBOUTIL_HPP
