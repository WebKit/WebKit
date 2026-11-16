/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
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
 * \brief String Query tests.
 *//*--------------------------------------------------------------------*/

#include "es2fStringQueryTests.hpp"
#include "es2fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "deString.h"

#include <algorithm>
#include <sstream>
#include <string>

using namespace glw; // GLint and other GL types

namespace deqp
{
namespace gles2
{
namespace Functional
{

StringQueryTests::StringQueryTests(Context &context) : TestCaseGroup(context, "string", "String Query tests")
{
}

StringQueryTests::~StringQueryTests(void)
{
}

void StringQueryTests::init(void)
{
    using tcu::TestLog;

    ES2F_ADD_API_CASE(renderer, "RENDERER", {
        const GLubyte *string = glGetString(GL_RENDERER);
        if (string == NULL)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid string");
    });
    ES2F_ADD_API_CASE(vendor, "VENDOR", {
        const GLubyte *string = glGetString(GL_VENDOR);
        if (string == NULL)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid string");
    });
    ES2F_ADD_API_CASE(version, "VERSION", {
        const char *string           = (const char *)glGetString(GL_VERSION);
        const char referenceString[] = "OpenGL ES ";

        if (string == NULL)
            TCU_FAIL("Got invalid string");

        if (!deStringBeginsWith(string, referenceString))
            TCU_FAIL("Got invalid string prefix");

        {
            std::string tmpString;
            char versionDelimiter;
            int glMajor = 0;
            int glMinor = 0;

            std::istringstream versionStream(string);
            versionStream >> tmpString; // OpenGL
            versionStream >> tmpString; // ES
            versionStream >> glMajor;   // x
            versionStream >> std::noskipws;
            versionStream >> versionDelimiter; // .
            versionStream >> glMinor;          // x

            if (!versionStream)
                TCU_FAIL("Got invalid string format");
        }
    });
    ES2F_ADD_API_CASE(shading_language_version, "SHADING_LANGUAGE_VERSION", {
        const char *string           = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
        const char referenceString[] = "OpenGL ES GLSL ES ";

        if (string == NULL)
            TCU_FAIL("Got invalid string");

        if (!deStringBeginsWith(string, referenceString))
            TCU_FAIL("Got invalid string prefix");

        {
            std::string tmpString;
            char versionDelimiter;
            int glslMajor = 0;
            int glslMinor = 0;

            std::istringstream versionStream(string);
            versionStream >> tmpString; // OpenGL
            versionStream >> tmpString; // ES
            versionStream >> tmpString; // GLSL
            versionStream >> tmpString; // ES
            versionStream >> glslMajor; // x
            versionStream >> std::noskipws;
            versionStream >> versionDelimiter; // .
            versionStream >> glslMinor;        // x

            if (!versionStream)
                TCU_FAIL("Got invalid string format");
        }
    });
    ES2F_ADD_API_CASE(extensions, "EXTENSIONS", {
        const char *extensions_cstring = (const char *)glGetString(GL_EXTENSIONS);
        if (extensions_cstring == NULL)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid string");
    });
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
