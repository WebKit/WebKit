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

#include "glsFboUtil.hpp"

#include "glwEnums.hpp"
#include "deUniquePtr.hpp"
#include "gluTextureUtil.hpp"
#include "gluStrUtil.hpp"
#include "deStringUtil.hpp"
#include "deSTLUtil.hpp"
#include <sstream>

using namespace glw;
using de::toString;
using de::UniquePtr;
using glu::ContextInfo;
using glu::ContextType;
using glu::getFramebufferAttachmentName;
using glu::getFramebufferAttachmentTypeName;
using glu::getFramebufferTargetName;
using glu::getTextureFormatName;
using glu::getTextureTargetName;
using glu::getTransferFormat;
using glu::getTypeName;
using glu::mapGLInternalFormat;
using glu::mapGLTransferFormat;
using glu::RenderContext;
using glu::TransferFormat;
using std::istream_iterator;
using std::istringstream;
using std::set;
using std::string;
using std::vector;
using tcu::NotSupportedError;
using tcu::TestLog;
using tcu::TextureFormat;

namespace deqp
{
namespace gls
{

namespace FboUtil
{

#if defined(DE_DEBUG)
static bool isFramebufferStatus(glw::GLenum fboStatus)
{
    return glu::getFramebufferStatusName(fboStatus) != nullptr;
}

static bool isErrorCode(glw::GLenum errorCode)
{
    return glu::getErrorName(errorCode) != nullptr;
}
#endif

std::ostream &operator<<(std::ostream &stream, const ImageFormat &format)
{
    if (format.unsizedType == GL_NONE)
    {
        // sized format
        return stream << glu::getTextureFormatStr(format.format);
    }
    else
    {
        // unsized format
        return stream << "(format = " << glu::getTextureFormatStr(format.format)
                      << ", type = " << glu::getTypeStr(format.unsizedType) << ")";
    }
}

void FormatDB::addCoreFormat(ImageFormat format, FormatFlags newFlags)
{
    FormatFlags &flags = m_formatFlags[format];
    flags              = FormatFlags(flags | newFlags);
}

void FormatDB::addExtensionFormat(ImageFormat format, FormatFlags newFlags,
                                  const std::set<std::string> &requiredExtensions)
{
    DE_ASSERT(!requiredExtensions.empty());

    {
        FormatFlags &flags = m_formatFlags[format];
        flags              = FormatFlags(flags | newFlags);
    }

    {
        std::set<ExtensionInfo> &extensionInfo = m_formatExtensions[format];
        ExtensionInfo extensionRecord;

        extensionRecord.flags              = newFlags;
        extensionRecord.requiredExtensions = requiredExtensions;

        DE_ASSERT(!de::contains(extensionInfo, extensionRecord)); // extensions specified only once
        extensionInfo.insert(extensionRecord);
    }
}

// Not too fast at the moment, might consider indexing?
Formats FormatDB::getFormats(FormatFlags requirements) const
{
    Formats ret;
    for (FormatMap::const_iterator it = m_formatFlags.begin(); it != m_formatFlags.end(); it++)
    {
        if ((it->second & requirements) == requirements)
            ret.insert(it->first);
    }
    return ret;
}

bool FormatDB::isKnownFormat(ImageFormat format) const
{
    return de::contains(m_formatFlags, format);
}

FormatFlags FormatDB::getFormatInfo(ImageFormat format) const
{
    DE_ASSERT(de::contains(m_formatFlags, format));
    return de::lookup(m_formatFlags, format);
}

std::set<std::set<std::string>> FormatDB::getFormatFeatureExtensions(ImageFormat format, FormatFlags requirements) const
{
    DE_ASSERT(de::contains(m_formatExtensions, format));

    const std::set<ExtensionInfo> &extensionInfo = de::lookup(m_formatExtensions, format);
    std::set<std::set<std::string>> ret;

    for (std::set<ExtensionInfo>::const_iterator it = extensionInfo.begin(); it != extensionInfo.end(); ++it)
    {
        if ((it->flags & requirements) == requirements)
            ret.insert(it->requiredExtensions);
    }

    return ret;
}

bool FormatDB::ExtensionInfo::operator<(const ExtensionInfo &other) const
{
    return (requiredExtensions < other.requiredExtensions) ||
           ((requiredExtensions == other.requiredExtensions) && (flags < other.flags));
}

static bool detectGLESCompatibleContext(const RenderContext &ctx, int requiredMajor, int requiredMinor)
{
    const glw::Functions &gl = ctx.getFunctions();
    glw::GLint majorVersion  = 0;
    glw::GLint minorVersion  = 0;

    // Detect compatible GLES context by querying GL_MAJOR_VERSION.
    // This query does not exist on GLES2 so a failing query implies
    // GLES2 context.

    gl.getIntegerv(GL_MAJOR_VERSION, &majorVersion);
    if (gl.getError() != GL_NO_ERROR)
        majorVersion = 2;

    gl.getIntegerv(GL_MINOR_VERSION, &minorVersion);
    if (gl.getError() != GL_NO_ERROR)
        minorVersion = 0;

    return (majorVersion > requiredMajor) || (majorVersion == requiredMajor && minorVersion >= requiredMinor);
}

static bool checkExtensionSupport(const ContextInfo &ctxInfo, const RenderContext &ctx, const std::string &extension)
{
    if (de::beginsWith(extension, "GL_"))
        return ctxInfo.isExtensionSupported(extension.c_str());
    else if (extension == "DEQP_gles3_core_compatible")
        return detectGLESCompatibleContext(ctx, 3, 0);
    else if (extension == "DEQP_gles31_core_compatible")
        return detectGLESCompatibleContext(ctx, 3, 1);
    else
    {
        DE_ASSERT(false);
        return false;
    }
}

bool checkExtensionSupport(const RenderContext &ctx, const std::string &extension)
{
    const de::UniquePtr<ContextInfo> info(ContextInfo::create(ctx));
    return checkExtensionSupport(*info, ctx, extension);
}

std::string getExtensionDescription(const std::string &extension)
{
    if (de::beginsWith(extension, "GL_"))
        return extension;
    else if (extension == "DEQP_gles3_core_compatible")
        return "GLES3 compatible context";
    else if (extension == "DEQP_gles31_core_compatible")
        return "GLES3.1 compatible context";
    else
    {
        DE_ASSERT(false);
        return "";
    }
}

void addFormats(FormatDB &db, FormatEntries stdFmts)
{
    for (const FormatEntry *it = stdFmts.begin(); it != stdFmts.end(); it++)
    {
        for (const FormatKey *it2 = it->second.begin(); it2 != it->second.end(); it2++)
            db.addCoreFormat(formatKeyInfo(*it2), it->first);
    }
}

void addExtFormats(FormatDB &db, FormatExtEntries extFmts, const RenderContext *ctx)
{
    const UniquePtr<ContextInfo> ctxInfo(ctx != nullptr ? ContextInfo::create(*ctx) : nullptr);
    for (const FormatExtEntry *entryIt = extFmts.begin(); entryIt != extFmts.end(); entryIt++)
    {
        bool supported = true;
        std::set<std::string> requiredExtensions;

        // parse required extensions
        {
            istringstream tokenStream(string(entryIt->extensions));
            istream_iterator<string> tokens((tokenStream)), end;

            while (tokens != end)
            {
                requiredExtensions.insert(*tokens);
                ++tokens;
            }
        }

        // check support
        if (ctxInfo)
        {
            for (std::set<std::string>::const_iterator extIt = requiredExtensions.begin();
                 extIt != requiredExtensions.end(); ++extIt)
            {
                if (!checkExtensionSupport(*ctxInfo, *ctx, *extIt))
                {
                    supported = false;
                    break;
                }
            }
        }

        if (supported)
            for (const FormatKey *i2 = entryIt->formats.begin(); i2 != entryIt->formats.end(); i2++)
                db.addExtensionFormat(formatKeyInfo(*i2), FormatFlags(entryIt->flags), requiredExtensions);
    }
}

FormatFlags formatFlag(GLenum context)
{
    switch (context)
    {
    case GL_NONE:
        return FormatFlags(0);
    case GL_RENDERBUFFER:
        return RENDERBUFFER_VALID;
    case GL_TEXTURE:
        return TEXTURE_VALID;
    case GL_STENCIL_ATTACHMENT:
        return STENCIL_RENDERABLE;
    case GL_DEPTH_ATTACHMENT:
        return DEPTH_RENDERABLE;
    default:
        DE_ASSERT(context >= GL_COLOR_ATTACHMENT0 && context <= GL_COLOR_ATTACHMENT15);
        return COLOR_RENDERABLE;
    }
}

static FormatFlags getAttachmentRenderabilityFlag(GLenum attachment)
{
    switch (attachment)
    {
    case GL_STENCIL_ATTACHMENT:
        return STENCIL_RENDERABLE;
    case GL_DEPTH_ATTACHMENT:
        return DEPTH_RENDERABLE;

    default:
        DE_ASSERT(attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15);
        return COLOR_RENDERABLE;
    }
}

namespace config
{

GLsizei imageNumSamples(const Image &img)
{
    if (const Renderbuffer *rbo = dynamic_cast<const Renderbuffer *>(&img))
        return rbo->numSamples;
    return 0;
}

static GLenum glTarget(const Image &img)
{
    if (dynamic_cast<const Renderbuffer *>(&img) != nullptr)
        return GL_RENDERBUFFER;
    if (dynamic_cast<const Texture2D *>(&img) != nullptr)
        return GL_TEXTURE_2D;
    if (dynamic_cast<const TextureCubeMap *>(&img) != nullptr)
        return GL_TEXTURE_CUBE_MAP;
    if (dynamic_cast<const Texture3D *>(&img) != nullptr)
        return GL_TEXTURE_3D;
    if (dynamic_cast<const Texture2DArray *>(&img) != nullptr)
        return GL_TEXTURE_2D_ARRAY;

    DE_FATAL("Impossible image type");
    return GL_NONE;
}

static void glInitFlat(const TextureFlat &cfg, GLenum target, const glw::Functions &gl)
{
    const TransferFormat format = transferImageFormat(cfg.internalFormat);
    GLint w                     = cfg.width;
    GLint h                     = cfg.height;
    for (GLint level = 0; level < cfg.numLevels; level++)
    {
        gl.texImage2D(target, level, cfg.internalFormat.format, w, h, 0, format.format, format.dataType, nullptr);
        w = de::max(1, w / 2);
        h = de::max(1, h / 2);
    }
}

static void glInitLayered(const TextureLayered &cfg, GLint depth_divider, const glw::Functions &gl)
{
    const TransferFormat format = transferImageFormat(cfg.internalFormat);
    GLint w                     = cfg.width;
    GLint h                     = cfg.height;
    GLint depth                 = cfg.numLayers;
    for (GLint level = 0; level < cfg.numLevels; level++)
    {
        gl.texImage3D(glTarget(cfg), level, cfg.internalFormat.format, w, h, depth, 0, format.format, format.dataType,
                      nullptr);
        w     = de::max(1, w / 2);
        h     = de::max(1, h / 2);
        depth = de::max(1, depth / depth_divider);
    }
}

static void glInit(const Texture &cfg, const glw::Functions &gl)
{
    if (const Texture2D *t2d = dynamic_cast<const Texture2D *>(&cfg))
        glInitFlat(*t2d, glTarget(*t2d), gl);
    else if (const TextureCubeMap *tcm = dynamic_cast<const TextureCubeMap *>(&cfg))
    {
        // \todo [2013-12-05 lauri]
        // move this to glu or someplace sensible (this array is already
        // present in duplicates)
        static const GLenum s_cubeMapFaces[] = {
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        };
        const Range<GLenum> range = GLS_ARRAY_RANGE(s_cubeMapFaces);
        for (const GLenum *it = range.begin(); it != range.end(); it++)
            glInitFlat(*tcm, *it, gl);
    }
    else if (const Texture3D *t3d = dynamic_cast<const Texture3D *>(&cfg))
        glInitLayered(*t3d, 2, gl);
    else if (const Texture2DArray *t2a = dynamic_cast<const Texture2DArray *>(&cfg))
        glInitLayered(*t2a, 1, gl);
}

static GLuint glCreate(const Image &cfg, const glw::Functions &gl)
{
    GLuint ret = 0;
    if (const Renderbuffer *const rbo = dynamic_cast<const Renderbuffer *>(&cfg))
    {
        gl.genRenderbuffers(1, &ret);
        gl.bindRenderbuffer(GL_RENDERBUFFER, ret);

        if (rbo->numSamples == 0)
            gl.renderbufferStorage(GL_RENDERBUFFER, rbo->internalFormat.format, rbo->width, rbo->height);
        else
            gl.renderbufferStorageMultisample(GL_RENDERBUFFER, rbo->numSamples, rbo->internalFormat.format, rbo->width,
                                              rbo->height);

        gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    else if (const Texture *const tex = dynamic_cast<const Texture *>(&cfg))
    {
        gl.genTextures(1, &ret);
        gl.bindTexture(glTarget(*tex), ret);
        glInit(*tex, gl);
        gl.bindTexture(glTarget(*tex), 0);
    }
    else
        DE_FATAL("Impossible image type");
    return ret;
}

static void glDelete(const Image &cfg, GLuint img, const glw::Functions &gl)
{
    if (dynamic_cast<const Renderbuffer *>(&cfg) != nullptr)
        gl.deleteRenderbuffers(1, &img);
    else if (dynamic_cast<const Texture *>(&cfg) != nullptr)
        gl.deleteTextures(1, &img);
    else
        DE_FATAL("Impossible image type");
}

static void attachAttachment(const Attachment &att, GLenum attPoint, const glw::Functions &gl)
{
    if (const RenderbufferAttachment *const rAtt = dynamic_cast<const RenderbufferAttachment *>(&att))
        gl.framebufferRenderbuffer(rAtt->target, attPoint, rAtt->renderbufferTarget, rAtt->imageName);
    else if (const TextureFlatAttachment *const fAtt = dynamic_cast<const TextureFlatAttachment *>(&att))
        gl.framebufferTexture2D(fAtt->target, attPoint, fAtt->texTarget, fAtt->imageName, fAtt->level);
    else if (const TextureLayerAttachment *const lAtt = dynamic_cast<const TextureLayerAttachment *>(&att))
        gl.framebufferTextureLayer(lAtt->target, attPoint, lAtt->imageName, lAtt->level, lAtt->layer);
    else
        DE_FATAL("Impossible attachment type");
}

GLenum attachmentType(const Attachment &att)
{
    if (dynamic_cast<const RenderbufferAttachment *>(&att) != nullptr)
        return GL_RENDERBUFFER;
    else if (dynamic_cast<const TextureAttachment *>(&att) != nullptr)
        return GL_TEXTURE;

    DE_FATAL("Impossible attachment type");
    return GL_NONE;
}

static GLsizei textureLayer(const TextureAttachment &tAtt)
{
    if (dynamic_cast<const TextureFlatAttachment *>(&tAtt) != nullptr)
        return 0;
    else if (const TextureLayerAttachment *const lAtt = dynamic_cast<const TextureLayerAttachment *>(&tAtt))
        return lAtt->layer;

    DE_FATAL("Impossible attachment type");
    return 0;
}

static void checkAttachmentCompleteness(Checker &cctx, const Attachment &attachment, GLenum attPoint,
                                        const Image *image, const FormatDB &db)
{
    // GLES2 4.4.5 / GLES3 4.4.4, "Framebuffer attachment completeness"

    if (const TextureAttachment *const texAtt = dynamic_cast<const TextureAttachment *>(&attachment))
        if (const TextureLayered *const ltex = dynamic_cast<const TextureLayered *>(image))
        {
            // GLES3: "If the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is
            // TEXTURE and the value of FRAMEBUFFER_ATTACHMENT_OBJECT_NAME names a
            // three-dimensional texture, then the value of
            // FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER must be smaller than the depth
            // of the texture.
            //
            // GLES3: "If the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is
            // TEXTURE and the value of FRAMEBUFFER_ATTACHMENT_OBJECT_NAME names a
            // two-dimensional array texture, then the value of
            // FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER must be smaller than the
            // number of layers in the texture.

            if (textureLayer(*texAtt) >= ltex->numLayers)
                cctx.addFBOStatus(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, "Attached layer index is larger than present");
        }

    // "The width and height of image are non-zero."
    if (image->width == 0 || image->height == 0)
        cctx.addFBOStatus(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, "Width and height of an image are not non-zero");

    // Check for renderability
    if (db.isKnownFormat(image->internalFormat))
    {
        const FormatFlags flags = db.getFormatInfo(image->internalFormat);

        // If the format does not have the proper renderability flag, the
        // completeness check _must_ fail.
        if ((flags & getAttachmentRenderabilityFlag(attPoint)) == 0)
            cctx.addFBOStatus(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                              "Attachment format is not renderable in this attachment");
        // If the format is only optionally renderable, the completeness check _can_ fail.
        else if ((flags & REQUIRED_RENDERABLE) == 0)
            cctx.addPotentialFBOStatus(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                                       "Attachment format is not required renderable");
    }
    else
        cctx.addFBOStatus(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, "Attachment format is not legal");
}

} // namespace config

using namespace config;

Checker::Checker(const glu::RenderContext &ctx, const FormatDB &formats) : m_renderCtx(ctx), m_formats(formats)
{
    m_statusCodes.setAllowComplete(true);
}

void Checker::addGLError(glw::GLenum error, const char *description)
{
    m_statusCodes.addErrorCode(error, description);
    m_statusCodes.setAllowComplete(false);
}

void Checker::addPotentialGLError(glw::GLenum error, const char *description)
{
    m_statusCodes.addErrorCode(error, description);
}

void Checker::addFBOStatus(GLenum status, const char *description)
{
    m_statusCodes.addFBOErrorStatus(status, description);
    m_statusCodes.setAllowComplete(false);
}

void Checker::addPotentialFBOStatus(GLenum status, const char *description)
{
    m_statusCodes.addFBOErrorStatus(status, description);
}

FboVerifier::FboVerifier(const FormatDB &formats, CheckerFactory &factory, const glu::RenderContext &renderCtx)
    : m_formats(formats)
    , m_factory(factory)
    , m_renderCtx(renderCtx)
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Return acceptable framebuffer status codes.
 *
 * This function examines the framebuffer configuration descriptor `fboConfig`
 * and returns the set of status codes that `glCheckFramebufferStatus` is
 * allowed to return on a conforming implementation when given a framebuffer
 * whose configuration adheres to `fboConfig`.
 *
 * The returned set is guaranteed to be non-empty, but it may contain multiple
 * INCOMPLETE statuses (if there are multiple errors in the spec), or or a mix
 * of COMPLETE and INCOMPLETE statuses (if supporting a FBO with this spec is
 * optional). Furthermore, the statuses may contain GL error codes, which
 * indicate that trying to create a framebuffer configuration like this could
 * have failed with an error (if one was checked for) even before
 * `glCheckFramebufferStatus` was ever called.
 *
 *//*--------------------------------------------------------------------*/
ValidStatusCodes FboVerifier::validStatusCodes(const Framebuffer &fboConfig) const
{
    const AttachmentMap &atts = fboConfig.attachments;
    const UniquePtr<Checker> cctx(m_factory.createChecker(m_renderCtx, m_formats));

    for (TextureMap::const_iterator it = fboConfig.textures.begin(); it != fboConfig.textures.end(); it++)
    {
        std::string errorDescription;

        if (m_formats.isKnownFormat(it->second->internalFormat))
        {
            const FormatFlags flags = m_formats.getFormatInfo(it->second->internalFormat);

            if ((flags & TEXTURE_VALID) == 0)
                errorDescription =
                    "Format " + de::toString(it->second->internalFormat) + " is not a valid format for a texture";
        }
        else if (it->second->internalFormat.unsizedType == GL_NONE)
        {
            // sized format
            errorDescription = "Format " + de::toString(it->second->internalFormat) + " does not exist";
        }
        else
        {
            // unsized type-format pair
            errorDescription = "Format " + de::toString(it->second->internalFormat) + " is not a legal format";
        }

        if (!errorDescription.empty())
        {
            cctx->addGLError(GL_INVALID_ENUM, errorDescription.c_str());
            cctx->addGLError(GL_INVALID_OPERATION, errorDescription.c_str());
            cctx->addGLError(GL_INVALID_VALUE, errorDescription.c_str());
        }
    }

    for (RboMap::const_iterator it = fboConfig.rbos.begin(); it != fboConfig.rbos.end(); it++)
    {
        if (m_formats.isKnownFormat(it->second->internalFormat))
        {
            const FormatFlags flags = m_formats.getFormatInfo(it->second->internalFormat);
            if ((flags & RENDERBUFFER_VALID) == 0)
            {
                const std::string reason =
                    "Format " + de::toString(it->second->internalFormat) + " is not a valid format for a renderbuffer";
                cctx->addGLError(GL_INVALID_ENUM, reason.c_str());
            }
        }
        else
        {
            const std::string reason =
                "Internal format " + de::toString(it->second->internalFormat) + " does not exist";
            cctx->addGLError(GL_INVALID_ENUM, reason.c_str());
        }
    }

    // "There is at least one image attached to the framebuffer."
    // \todo support XXX_framebuffer_no_attachments
    if (atts.empty())
        cctx->addFBOStatus(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT, "No images attached to the framebuffer");

    for (AttachmentMap::const_iterator it = atts.begin(); it != atts.end(); it++)
    {
        const GLenum attPoint    = it->first;
        const Attachment &att    = *it->second;
        const Image *const image = fboConfig.getImage(attachmentType(att), att.imageName);

        checkAttachmentCompleteness(*cctx, att, attPoint, image, m_formats);
        cctx->check(it->first, *it->second, image);
    }

    return cctx->getStatusCodes();
}

void Framebuffer::attach(glw::GLenum attPoint, const Attachment *att)
{
    if (att == nullptr)
        attachments.erase(attPoint);
    else
        attachments[attPoint] = att;
}

const Image *Framebuffer::getImage(GLenum type, glw::GLuint imgName) const
{
    switch (type)
    {
    case GL_TEXTURE:
        return de::lookupDefault(textures, imgName, nullptr);
    case GL_RENDERBUFFER:
        return de::lookupDefault(rbos, imgName, nullptr);
    default:
        DE_FATAL("Bad image type");
    }
    return nullptr; // shut up compiler warning
}

void Framebuffer::setTexture(glw::GLuint texName, const Texture &texCfg)
{
    textures[texName] = &texCfg;
}

void Framebuffer::setRbo(glw::GLuint rbName, const Renderbuffer &rbCfg)
{
    rbos[rbName] = &rbCfg;
}

static void logField(TestLog &log, const string &field, const string &value)
{
    log << TestLog::Message << field << ": " << value << TestLog::EndMessage;
}

static void logImage(const Image &img, TestLog &log, bool useType)
{
    const GLenum type = img.internalFormat.unsizedType;
    logField(log, "Internal format", getTextureFormatName(img.internalFormat.format));
    if (useType && type != GL_NONE)
        logField(log, "Format type", getTypeName(type));
    logField(log, "Width", toString(img.width));
    logField(log, "Height", toString(img.height));
}

static void logRenderbuffer(const Renderbuffer &rbo, TestLog &log)
{
    logImage(rbo, log, false);
    logField(log, "Samples", toString(rbo.numSamples));
}

static void logTexture(const Texture &tex, TestLog &log)
{
    logField(log, "Type", glu::getTextureTargetName(glTarget(tex)));
    logImage(tex, log, true);
    logField(log, "Levels", toString(tex.numLevels));
    if (const TextureLayered *const lTex = dynamic_cast<const TextureLayered *>(&tex))
        logField(log, "Layers", toString(lTex->numLayers));
}

static void logAttachment(const Attachment &att, TestLog &log)
{
    logField(log, "Target", getFramebufferTargetName(att.target));
    logField(log, "Type", getFramebufferAttachmentTypeName(attachmentType(att)));
    logField(log, "Image Name", toString(att.imageName));
    if (const RenderbufferAttachment *const rAtt = dynamic_cast<const RenderbufferAttachment *>(&att))
    {
        DE_UNREF(rAtt); // To shut up compiler during optimized builds.
        DE_ASSERT(rAtt->renderbufferTarget == GL_RENDERBUFFER);
        logField(log, "Renderbuffer Target", "GL_RENDERBUFFER");
    }
    else if (const TextureAttachment *const tAtt = dynamic_cast<const TextureAttachment *>(&att))
    {
        logField(log, "Mipmap Level", toString(tAtt->level));
        if (const TextureFlatAttachment *const fAtt = dynamic_cast<const TextureFlatAttachment *>(tAtt))
            logField(log, "Texture Target", getTextureTargetName(fAtt->texTarget));
        else if (const TextureLayerAttachment *const lAtt = dynamic_cast<const TextureLayerAttachment *>(tAtt))
            logField(log, "Layer", toString(lAtt->level));
    }
}

void logFramebufferConfig(const Framebuffer &cfg, TestLog &log)
{
    log << TestLog::Section("Framebuffer", "Framebuffer configuration");

    for (RboMap::const_iterator it = cfg.rbos.begin(); it != cfg.rbos.end(); ++it)
    {
        const string num = toString(it->first);
        const tcu::ScopedLogSection subsection(log, num, "Renderbuffer " + num);

        logRenderbuffer(*it->second, log);
    }

    for (TextureMap::const_iterator it = cfg.textures.begin(); it != cfg.textures.end(); ++it)
    {
        const string num = toString(it->first);
        const tcu::ScopedLogSection subsection(log, num, "Texture " + num);

        logTexture(*it->second, log);
    }

    const string attDesc = cfg.attachments.empty() ? "Framebuffer has no attachments" : "Framebuffer attachments";
    log << TestLog::Section("Attachments", attDesc);
    for (AttachmentMap::const_iterator it = cfg.attachments.begin(); it != cfg.attachments.end(); it++)
    {
        const string attPointName = getFramebufferAttachmentName(it->first);
        log << TestLog::Section(attPointName, "Attachment point " + attPointName);
        logAttachment(*it->second, log);
        log << TestLog::EndSection;
    }
    log << TestLog::EndSection; // Attachments

    log << TestLog::EndSection; // Framebuffer
}

ValidStatusCodes::ValidStatusCodes(void) : m_allowComplete(false)
{
}

bool ValidStatusCodes::isFBOStatusValid(glw::GLenum fboStatus) const
{
    if (fboStatus == GL_FRAMEBUFFER_COMPLETE)
        return m_allowComplete;
    else
    {
        // rule violation exists?
        for (int ndx = 0; ndx < (int)m_errorStatuses.size(); ++ndx)
        {
            if (m_errorStatuses[ndx].errorCode == fboStatus)
                return true;
        }
        return false;
    }
}

bool ValidStatusCodes::isFBOStatusRequired(glw::GLenum fboStatus) const
{
    if (fboStatus == GL_FRAMEBUFFER_COMPLETE)
        return m_allowComplete && m_errorStatuses.empty();
    else
        // fboStatus is the only allowed error status and succeeding is forbidden
        return !m_allowComplete && m_errorStatuses.size() == 1 && m_errorStatuses.front().errorCode == fboStatus;
}

bool ValidStatusCodes::isErrorCodeValid(glw::GLenum errorCode) const
{
    if (errorCode == GL_NO_ERROR)
        return m_errorCodes.empty();
    else
    {
        // rule violation exists?
        for (int ndx = 0; ndx < (int)m_errorCodes.size(); ++ndx)
        {
            if (m_errorCodes[ndx].errorCode == errorCode)
                return true;
        }
        return false;
    }
}

bool ValidStatusCodes::isErrorCodeRequired(glw::GLenum errorCode) const
{
    if (m_errorCodes.empty() && errorCode == GL_NO_ERROR)
        return true;
    else
        // only this error code listed
        return m_errorCodes.size() == 1 && m_errorCodes.front().errorCode == errorCode;
}

void ValidStatusCodes::addErrorCode(glw::GLenum error, const char *description)
{
    DE_ASSERT(isErrorCode(error));
    DE_ASSERT(error != GL_NO_ERROR);
    addViolation(m_errorCodes, error, description);
}

void ValidStatusCodes::addFBOErrorStatus(glw::GLenum status, const char *description)
{
    DE_ASSERT(isFramebufferStatus(status));
    DE_ASSERT(status != GL_FRAMEBUFFER_COMPLETE);
    addViolation(m_errorStatuses, status, description);
}

void ValidStatusCodes::setAllowComplete(bool b)
{
    m_allowComplete = b;
}

void ValidStatusCodes::logLegalResults(tcu::TestLog &log) const
{
    tcu::MessageBuilder msg(&log);
    std::vector<std::string> validResults;

    for (int ndx = 0; ndx < (int)m_errorCodes.size(); ++ndx)
        validResults.push_back(std::string(glu::getErrorName(m_errorCodes[ndx].errorCode)) +
                               " (during FBO initialization)");

    for (int ndx = 0; ndx < (int)m_errorStatuses.size(); ++ndx)
        validResults.push_back(glu::getFramebufferStatusName(m_errorStatuses[ndx].errorCode));

    if (m_allowComplete)
        validResults.push_back("GL_FRAMEBUFFER_COMPLETE");

    msg << "Expected ";
    if (validResults.size() > 1)
        msg << "one of ";

    for (int ndx = 0; ndx < (int)validResults.size(); ++ndx)
    {
        const bool last         = ((ndx + 1) == (int)validResults.size());
        const bool secondToLast = ((ndx + 2) == (int)validResults.size());

        msg << validResults[ndx];
        if (!last)
            msg << ((secondToLast) ? (" or ") : (", "));
    }

    msg << "." << TestLog::EndMessage;
}

void ValidStatusCodes::logRules(tcu::TestLog &log) const
{
    const tcu::ScopedLogSection section(log, "Rules", "Active rules");

    for (int ndx = 0; ndx < (int)m_errorCodes.size(); ++ndx)
        logRule(log, glu::getErrorName(m_errorCodes[ndx].errorCode), m_errorCodes[ndx].rules);

    for (int ndx = 0; ndx < (int)m_errorStatuses.size(); ++ndx)
        logRule(log, glu::getFramebufferStatusName(m_errorStatuses[ndx].errorCode), m_errorStatuses[ndx].rules);

    if (m_allowComplete)
    {
        std::set<std::string> defaultRule;
        defaultRule.insert("FBO is complete");
        logRule(log, "GL_FRAMEBUFFER_COMPLETE", defaultRule);
    }
}

void ValidStatusCodes::logRule(tcu::TestLog &log, const std::string &ruleName, const std::set<std::string> &rules) const
{
    if (!rules.empty())
    {
        const tcu::ScopedLogSection section(log, ruleName, ruleName);
        tcu::MessageBuilder msg(&log);

        msg << "Rules:\n";
        for (std::set<std::string>::const_iterator it = rules.begin(); it != rules.end(); ++it)
            msg << "\t * " << *it << "\n";
        msg << TestLog::EndMessage;
    }
}

void ValidStatusCodes::addViolation(std::vector<RuleViolation> &dst, glw::GLenum code, const char *description) const
{
    // rule violation already exists?
    for (int ndx = 0; ndx < (int)dst.size(); ++ndx)
    {
        if (dst[ndx].errorCode == code)
        {
            dst[ndx].rules.insert(std::string(description));
            return;
        }
    }

    // new violation
    {
        RuleViolation violation;

        violation.errorCode = code;
        violation.rules.insert(std::string(description));

        dst.push_back(violation);
    }
}

FboBuilder::FboBuilder(GLuint fbo, GLenum target, const glw::Functions &gl)
    : m_error(GL_NO_ERROR)
    , m_target(target)
    , m_gl(gl)
{
    m_gl.bindFramebuffer(m_target, fbo);
}

FboBuilder::~FboBuilder(void)
{
    for (TextureMap::const_iterator it = textures.begin(); it != textures.end(); it++)
    {
        glDelete(*it->second, it->first, m_gl);
    }
    for (RboMap::const_iterator it = rbos.begin(); it != rbos.end(); it++)
    {
        glDelete(*it->second, it->first, m_gl);
    }
    m_gl.bindFramebuffer(m_target, 0);
    for (Configs::const_iterator it = m_configs.begin(); it != m_configs.end(); it++)
    {
        delete *it;
    }
}

void FboBuilder::checkError(void)
{
    const GLenum error = m_gl.getError();
    if (error != GL_NO_ERROR && m_error == GL_NO_ERROR)
        m_error = error;
}

void FboBuilder::glAttach(GLenum attPoint, const Attachment *att)
{
    if (att == NULL)
        m_gl.framebufferRenderbuffer(m_target, attPoint, GL_RENDERBUFFER, 0);
    else
        attachAttachment(*att, attPoint, m_gl);
    checkError();
    attach(attPoint, att);
}

GLuint FboBuilder::glCreateTexture(const Texture &texCfg)
{
    const GLuint texName = glCreate(texCfg, m_gl);
    checkError();
    setTexture(texName, texCfg);
    return texName;
}

GLuint FboBuilder::glCreateRbo(const Renderbuffer &rbCfg)
{
    const GLuint rbName = glCreate(rbCfg, m_gl);
    checkError();
    setRbo(rbName, rbCfg);
    return rbName;
}

TransferFormat transferImageFormat(const ImageFormat &imgFormat)
{
    if (imgFormat.unsizedType == GL_NONE)
        return getTransferFormat(mapGLInternalFormat(imgFormat.format));
    else
        return TransferFormat(imgFormat.format, imgFormat.unsizedType);
}

} // namespace FboUtil
} // namespace gls
} // namespace deqp
