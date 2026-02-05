//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FunctionsGL.cpp: Implements the FuntionsGL class to contain loaded GL functions

#include "libANGLE/renderer/gl/FunctionsGL.h"

#include <algorithm>

#include "common/string_utils.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace rx
{

static void GetGLVersion(PFNGLGETSTRINGPROC getStringFunction,
                         gl::Version *outVersion,
                         StandardGL *outStandard)
{
    const std::string version = reinterpret_cast<const char *>(getStringFunction(GL_VERSION));
    if (version.find("OpenGL ES") == std::string::npos)
    {
        // OpenGL spec states the GL_VERSION string will be in the following format:
        // <version number><space><vendor-specific information>
        // The version number is either of the form major number.minor number or major
        // number.minor number.release number, where the numbers all have one or more
        // digits
        *outStandard = STANDARD_GL_DESKTOP;
        *outVersion  = gl::Version(static_cast<uint8_t>(version[0] - '0'),
                                   static_cast<uint8_t>(version[2] - '0'));
    }
    else
    {
        // ES spec states that the GL_VERSION string will be in the following format:
        // "OpenGL ES N.M vendor-specific information"
        *outStandard = STANDARD_GL_ES;
        *outVersion  = gl::Version(static_cast<uint8_t>(version[10] - '0'),
                                   static_cast<uint8_t>(version[12] - '0'));
    }
}

static std::vector<std::string> GetIndexedExtensions(PFNGLGETINTEGERVPROC getIntegerFunction,
                                                     PFNGLGETSTRINGIPROC getStringIFunction)
{
    std::vector<std::string> result;

    GLint numExtensions;
    getIntegerFunction(GL_NUM_EXTENSIONS, &numExtensions);

    result.reserve(numExtensions);

    for (GLint i = 0; i < numExtensions; i++)
    {
        if (const char *extensionString =
                reinterpret_cast<const char *>(getStringIFunction(GL_EXTENSIONS, i)))
        {
            result.push_back(extensionString);
        }
    }

    return result;
}

#define ASSIGN(NAME, FP) FP = reinterpret_cast<decltype(FP)>(loadProcAddress(NAME))

FunctionsGL::FunctionsGL() : version(), standard(), extensions() {}

FunctionsGL::~FunctionsGL() {}

void FunctionsGL::initialize(const egl::AttributeMap &displayAttributes)
{
    // Grab the version number
    ASSIGN("glGetString", getString);
    ASSIGN("glGetIntegerv", getIntegerv);
    GetGLVersion(getString, &version, &standard);

    // Grab the GL extensions
    if (isAtLeastGL(gl::Version(3, 0)) || isAtLeastGLES(gl::Version(3, 0)))
    {
        ASSIGN("glGetStringi", getStringi);
        extensions = GetIndexedExtensions(getIntegerv, getStringi);
    }
    else
    {
        const char *exts = reinterpret_cast<const char *>(getString(GL_EXTENSIONS));
        angle::SplitStringAlongWhitespace(std::string(exts), &extensions);
    }

    std::set<std::string> extensionSet;
    for (const auto &extension : extensions)
    {
        extensionSet.insert(extension);
    }

    // Note:
    // Even though extensions are written against specific versions of GL, many drivers expose the
    // extensions in even older versions.  Always try loading the extensions regardless of GL
    // version.

    // Load the entry points

    switch (standard)
    {
        case STANDARD_GL_DESKTOP:
        {
            // Check the context profile
            profile = 0;
            if (isAtLeastGL(gl::Version(3, 2)))
            {
                getIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
            }

            initProcsDesktopGL(version, extensionSet);
            // Test that ANGLE_ENABLE_GL_DESKTOP_BACKEND has been enabled
            // See http://anglebug.com/42266631
            ASSERT(getString != nullptr && getError != nullptr);
            break;
        }

        case STANDARD_GL_ES:
        {
            // No profiles in GLES
            profile = 0;
            initProcsGLES(version, extensionSet);
            break;
        }

        default:
            UNREACHABLE();
            break;
    }

    initProcsSharedExtensions(extensionSet);
}

bool FunctionsGL::isAtLeastGL(const gl::Version &glVersion) const
{
    return standard == STANDARD_GL_DESKTOP && version >= glVersion;
}

bool FunctionsGL::isAtMostGL(const gl::Version &glVersion) const
{
    return standard == STANDARD_GL_DESKTOP && glVersion >= version;
}

bool FunctionsGL::isAtLeastGLES(const gl::Version &glesVersion) const
{
    return standard == STANDARD_GL_ES && version >= glesVersion;
}

bool FunctionsGL::isAtMostGLES(const gl::Version &glesVersion) const
{
    return standard == STANDARD_GL_ES && glesVersion >= version;
}

bool FunctionsGL::hasExtension(const std::string &ext) const
{
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

bool FunctionsGL::hasGLExtension(const std::string &ext) const
{
    return standard == STANDARD_GL_DESKTOP && hasExtension(ext);
}

bool FunctionsGL::hasGLESExtension(const std::string &ext) const
{
    return standard == STANDARD_GL_ES && hasExtension(ext);
}
}  // namespace rx
