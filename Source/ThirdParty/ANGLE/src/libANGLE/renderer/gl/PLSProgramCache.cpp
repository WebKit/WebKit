//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PLSProgramCache.cpp: Implements a cache of native programs used to render load/store operations
// for EXT_shader_pixel_local_storage.

#include "libANGLE/renderer/gl/PLSProgramCache.h"

#include "libANGLE/Caps.h"
#include "libANGLE/PixelLocalStorage.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"

namespace rx
{
namespace
{
GLuint CreateVAO(const FunctionsGL *gl)
{
    GLuint vao;
    gl->genVertexArrays(1, &vao);
    return vao;
}

bool IsShaderCompiled(const FunctionsGL *gl, GLuint shaderID)
{
    GLint isCompiled = 0;
    gl->getShaderiv(shaderID, GL_COMPILE_STATUS, &isCompiled);
    return isCompiled != 0;
}

bool IsProgramLinked(const FunctionsGL *gl, GLuint programID)
{
    GLint isLinked = 0;
    gl->getProgramiv(programID, GL_LINK_STATUS, &isLinked);
    return isLinked != 0;
}

PLSFormatKey GetPLSFormatKey(GLenum internalformat)
{
    switch (internalformat)
    {
        default:
            UNREACHABLE();
            [[fallthrough]];
        case GL_NONE:
            return PLSFormatKey::INACTIVE;
        case GL_RGBA8:
            return PLSFormatKey::RGBA8;
        case GL_RGBA8I:
            return PLSFormatKey::RGBA8I;
        case GL_RGBA8UI:
            return PLSFormatKey::RGBA8UI;
        case GL_R32F:
            return PLSFormatKey::R32F;
        case GL_R32UI:
            return PLSFormatKey::R32UI;
    }
}

constexpr uint64_t BitRepeat(uint64_t bits, int interval)
{
    return interval >= 64 ? bits : BitRepeat(bits | (bits << interval), interval * 2);
}

const char *GetPLSQualifier(PLSProgramType type)
{
    switch (type)
    {
        case PLSProgramType::Load:
            return "__pixel_local_outEXT";
        case PLSProgramType::Store:
            return "__pixel_local_inEXT";
        default:
            UNREACHABLE();
            return "";
    }
}

const char *GetFormatQualifier(PLSFormatKey formatKey)
{
    switch (formatKey)
    {
        case PLSFormatKey::RGBA8:
            return "rgba8";
        case PLSFormatKey::RGBA8I:
            return "rgba8i";
        case PLSFormatKey::RGBA8UI:
            return "rgba8ui";
        case PLSFormatKey::R32F:
            return "r32f";
        case PLSFormatKey::R32UI:
            return "r32ui";
        default:
            UNREACHABLE();
            return "";
    }
}

const char *GetFormatPrecision(PLSFormatKey formatKey)
{
    switch (formatKey)
    {
        case PLSFormatKey::RGBA8:
        case PLSFormatKey::RGBA8I:
        case PLSFormatKey::RGBA8UI:
            return "lowp";
        case PLSFormatKey::R32F:
        case PLSFormatKey::R32UI:
            return "highp";
        default:
            UNREACHABLE();
            return "";
    }
}

const char *GetFormatRawType(PLSFormatKey formatKey)
{
    switch (formatKey)
    {
        case PLSFormatKey::RGBA8:
            return "vec4";
        case PLSFormatKey::RGBA8I:
            return "ivec4";
        case PLSFormatKey::RGBA8UI:
            return "uvec4";
        case PLSFormatKey::R32F:
            return "float";
        case PLSFormatKey::R32UI:
            return "uint";
        default:
            UNREACHABLE();
            return "";
    }
}

const char *GetFormatPrefix(PLSFormatKey formatKey)
{
    switch (formatKey)
    {
        case PLSFormatKey::RGBA8:
        case PLSFormatKey::R32F:
            return "";
        case PLSFormatKey::RGBA8I:
            return "i";
        case PLSFormatKey::RGBA8UI:
        case PLSFormatKey::R32UI:
            return "u";
        default:
            UNREACHABLE();
            return "";
    }
}

const char *GetFormatSwizzle(PLSFormatKey formatKey)
{
    switch (formatKey)
    {
        case PLSFormatKey::RGBA8:
        case PLSFormatKey::RGBA8I:
        case PLSFormatKey::RGBA8UI:
            return "";
        case PLSFormatKey::R32F:
        case PLSFormatKey::R32UI:
            return ".r";
        default:
            UNREACHABLE();
            return "";
    }
}

void ExpandPLSVar(std::ostringstream &out, int binding, PLSFormatKey formatKey)
{
    switch (formatKey)
    {
        case PLSFormatKey::RGBA8:
        case PLSFormatKey::RGBA8I:
        case PLSFormatKey::RGBA8UI:
            out << "pls._" << binding;
            break;
        case PLSFormatKey::R32F:
            out << "vec4(pls._" << binding << ",0,0,1)";
            break;
        case PLSFormatKey::R32UI:
            out << "uvec4(pls._" << binding << ",0,0,1)";
            break;
        default:
            UNREACHABLE();
    }
}
}  // namespace

PLSProgramCache::PLSProgramCache(const FunctionsGL *gl, const gl::Caps &nativeCaps)
    : mGL(gl),
      mVertexShaderID(mGL->createShader(GL_VERTEX_SHADER)),
      mEmptyVAO(CreateVAO(gl)),
      mEmptyVAOState(nativeCaps.maxVertexAttributes, nativeCaps.maxVertexAttribBindings),
      mCache(MaximumTotalPrograms)
{
    // Draws a fullscreen quad from a 4-point GL_TRIANGLE_STRIP.
    constexpr char kLoadPLSVertexShader[] = R"(#version 310 es
void main()
{
    gl_Position = vec4(mix(vec2(-1), vec2(1), equal(gl_VertexID & ivec2(1, 2), ivec2(0))), 0, 1);
})";
    const char *loadPLSVertexShaderPtr    = kLoadPLSVertexShader;
    mGL->shaderSource(mVertexShaderID, 1, &loadPLSVertexShaderPtr, nullptr);
    mGL->compileShader(mVertexShaderID);
    ASSERT(IsShaderCompiled(mGL, mVertexShaderID));
}

PLSProgramCache::~PLSProgramCache()
{
    mGL->deleteShader(mVertexShaderID);
    mGL->deleteVertexArrays(1, &mEmptyVAO);
}

void PLSProgramKeyBuilder::prependPlane(GLenum internalformat, bool preserved)
{
    uint64_t rawFormatKey = static_cast<uint64_t>(GetPLSFormatKey(internalformat));
    mRawKey               = (mRawKey << (PLSProgramKey::BitsPerPlane - 1)) | rawFormatKey;
    mRawKey               = (mRawKey << 1) | static_cast<uint64_t>(preserved);
}

PLSProgramKey PLSProgramKeyBuilder::finish(PLSProgramType type)
{
    return PLSProgramKey((mRawKey << 1) | static_cast<uint64_t>(type));
}

PLSProgramType PLSProgramKey::type() const
{
    return static_cast<PLSProgramType>(mRawKey & 1);
}

bool PLSProgramKey::areAnyPreserved() const
{
    // The bottom bit of the entire rawKey says whether the program is load or store.
    // The bottom bit in each individual plane sub-key says whether the plane is preserved.
    constexpr uint64_t PreserveMask = BitRepeat(1, BitsPerPlane) << 1;
    return (mRawKey & PreserveMask) != 0;
}

PLSProgramKey::Iter::Iter(const PLSProgramKey &key)
    : mPlaneKeys(key.rawKey() >> 1)  // The first rawKey bit says if the program is load or store.
{
    skipInactivePlanes();
}

PLSFormatKey PLSProgramKey::Iter::formatKey() const
{
    return static_cast<PLSFormatKey>((mPlaneKeys & SinglePlaneMask) >> 1);
}

bool PLSProgramKey::Iter::preserved() const
{
    return mPlaneKeys & 1;
}

bool PLSProgramKey::Iter::operator!=(const Iter &iter) const
{
    return mPlaneKeys != iter.mPlaneKeys;
}

void PLSProgramKey::Iter::operator++()
{
    ++mBinding;
    mPlaneKeys >>= BitsPerPlane;
    skipInactivePlanes();
}

void PLSProgramKey::Iter::skipInactivePlanes()
{
    // Skip over any planes that are not active. The only effect inactive planes have on shaders is
    // to offset the next binding index.
    if (mPlaneKeys != 0 && formatKey() == PLSFormatKey::INACTIVE)
    {
        ++*this;  // Recurses if there are adjacent inactive planes.
    }
}

PLSProgram::PLSProgram(const FunctionsGL *gl, GLuint vertexShaderID, PLSProgramKey key)
    : mGL(gl), mKey(key), mProgramID(mGL->createProgram())
{
    std::ostringstream fs;
    fs << "#version 310 es\n";
    fs << "#extension GL_EXT_shader_pixel_local_storage : require\n";

    // Emit the global pixel local storage interface block.
    fs << GetPLSQualifier(mKey.type()) << " PLS{";
    for (auto [binding, formatKey, preserved] : mKey)
    {
        fs << "layout(" << GetFormatQualifier(formatKey) << ")" << GetFormatPrecision(formatKey)
           << " " << GetFormatRawType(formatKey) << " _" << binding << ";";
    }
    fs << "}pls;\n";

    // Emit the sources/destinations of each PLS plane (either images or uniform clear values).
    for (auto [binding, formatKey, preserved] : mKey)
    {
        if (mKey.type() == PLSProgramType::Load)
        {
            if (preserved)
            {
                // Emit an image to load into the PLS plane.
                fs << "layout(binding=" << binding << "," << GetFormatQualifier(formatKey)
                   << ")uniform readonly " << GetFormatPrecision(formatKey) << " "
                   << GetFormatPrefix(formatKey) << "image2D i" << binding << ";";
            }
            else
            {
                // Emit a uniform clear value to initialize the PLS plane with.
                fs << "uniform " << GetFormatPrecision(formatKey) << " "
                   << GetFormatPrefix(formatKey) << "vec4 c" << binding << ";";
            }
        }
        else
        {
            if (preserved)
            {
                // Emit an image to dump the PLS plane to.
                fs << "layout(binding=" << binding << "," << GetFormatQualifier(formatKey)
                   << ")uniform writeonly " << GetFormatPrecision(formatKey) << " "
                   << GetFormatPrefix(formatKey) << "image2D i" << binding << ";";
            }
        }
    }

    fs << "void main(){";
    if (mKey.areAnyPreserved())
    {
        fs << "highp ivec2 pixelCoord=ivec2(floor(gl_FragCoord.xy));";
    }
    // Emit the load/store operations for each plane.
    for (auto [binding, formatKey, preserved] : mKey)
    {
        if (mKey.type() == PLSProgramType::Load)
        {
            fs << "pls._" << binding << "=";
            if (preserved)
            {
                fs << "imageLoad(i" << binding << ",pixelCoord)";
            }
            else
            {
                fs << "c" << binding;
            }
            fs << GetFormatSwizzle(formatKey) << ";";
        }
        else
        {
            if (preserved)
            {
                fs << "imageStore(i" << binding << ",pixelCoord,";
                ExpandPLSVar(fs, binding, formatKey);
                fs << ");";
            }
        }
    }
    fs << "}";

    // Compile.
    GLuint fragmentShaderID = mGL->createShader(GL_FRAGMENT_SHADER);
    std::string str         = fs.str();
    const char *source      = str.c_str();
    mGL->shaderSource(fragmentShaderID, 1, &source, nullptr);
    mGL->compileShader(fragmentShaderID);
    ASSERT(IsShaderCompiled(mGL, fragmentShaderID));

    // Link.
    mGL->attachShader(mProgramID, vertexShaderID);
    mGL->attachShader(mProgramID, fragmentShaderID);
    mGL->linkProgram(mProgramID);
    ASSERT(IsProgramLinked(mGL, mProgramID));

    mGL->deleteShader(fragmentShaderID);

    // Get the locations of uniform clear values.
    if (mKey.type() == PLSProgramType::Load)
    {
        for (auto [binding, formatKey, preserved] : mKey)
        {
            if (!preserved)
            {
                std::ostringstream name;
                name << "c" << binding;
                mClearValueUniformLocations[binding] = {
                    mGL->getUniformLocation(mProgramID, name.str().c_str())};
                ASSERT(mClearValueUniformLocations[binding] >= 0);
            }
        }
    }
}

PLSProgram::~PLSProgram()
{
    mGL->deleteProgram(mProgramID);
}

void PLSProgram::setClearValues(const gl::PixelLocalStoragePlane planes[],
                                const GLenum loadops[]) const
{
    ASSERT(mKey.type() == PLSProgramType::Load);

    // Updates uniforms representing clear values on the current program.
    class ClearUniformCommands : public gl::PixelLocalStoragePlane::ClearCommands
    {
      public:
        ClearUniformCommands(const FunctionsGL *gl, const GLint *clearValueUniformLocations)
            : mGL(gl), mClearValueUniformLocations(clearValueUniformLocations)
        {}

        void clearfv(int binding, const GLfloat value[]) const override
        {
            mGL->uniform4fv(mClearValueUniformLocations[binding], 1, value);
        }

        void cleariv(int binding, const GLint value[]) const override
        {
            mGL->uniform4iv(mClearValueUniformLocations[binding], 1, value);
        }

        void clearuiv(int binding, const GLuint value[]) const override
        {
            mGL->uniform4uiv(mClearValueUniformLocations[binding], 1, value);
        }

      private:
        const FunctionsGL *const mGL;
        const GLint *const mClearValueUniformLocations;
    };

    ClearUniformCommands clearUniformCommands(mGL, mClearValueUniformLocations.data());
    for (auto [binding, formatKey, preserved] : mKey)
    {
        if (!preserved)
        {
            planes[binding].issueClearCommand(&clearUniformCommands, binding, loadops[binding]);
        }
    }
}

const PLSProgram *PLSProgramCache::getProgram(PLSProgramKey key)
{
    const std::unique_ptr<PLSProgram> *programPtr;
    if (!mCache.get(key.rawKey(), &programPtr))
    {
        auto program = std::make_unique<PLSProgram>(mGL, mVertexShaderID, key);
        programPtr   = mCache.put(key.rawKey(), std::move(program), 1);
    }
    ASSERT(programPtr != nullptr);
    return programPtr->get();
}
}  // namespace rx
