#ifndef _ES3FFBOTESTCASE_HPP
#define _ES3FFBOTESTCASE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
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
 * \brief Base class for FBO tests.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tes3TestCase.hpp"
#include "sglrContextWrapper.hpp"

namespace tcu
{
class Surface;
class TextureFormat;
} // namespace tcu

namespace deqp
{
namespace gles3
{
namespace Functional
{

class FboTestCase : public TestCase, public sglr::ContextWrapper
{
public:
    FboTestCase(Context &context, const char *name, const char *description, bool useScreenSizedViewport = false);
    ~FboTestCase(void);

    IterateResult iterate(void);

protected:
    virtual void preCheck(void)
    {
    }
    virtual void render(tcu::Surface &dst) = 0;
    virtual bool compare(const tcu::Surface &reference, const tcu::Surface &result);

    // Utilities.
    void checkFormatSupport(uint32_t sizedFormat);
    void checkSampleCount(uint32_t sizedFormat, int numSamples);
    void readPixels(tcu::Surface &dst, int x, int y, int width, int height, const tcu::TextureFormat &format,
                    const tcu::Vec4 &scale, const tcu::Vec4 &bias);
    void readPixels(tcu::Surface &dst, int x, int y, int width, int height);
    void checkFramebufferStatus(uint32_t target);
    void checkError(void);
    void clearColorBuffer(const tcu::TextureFormat &format, const tcu::Vec4 &value = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f));

    int m_viewportWidth;
    int m_viewportHeight;

private:
    FboTestCase(const FboTestCase &other);
    FboTestCase &operator=(const FboTestCase &other);
};

} // namespace Functional
} // namespace gles3
} // namespace deqp

#endif // _ES3FFBOTESTCASE_HPP
