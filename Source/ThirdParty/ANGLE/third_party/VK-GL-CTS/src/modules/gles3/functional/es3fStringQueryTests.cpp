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
 * \brief String Query tests.
 *//*--------------------------------------------------------------------*/

#include "es3fStringQueryTests.hpp"
#include "es3fApiCase.hpp"
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
namespace gles3
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

    ES3F_ADD_API_CASE(renderer, "RENDERER", {
        const GLubyte *string = glGetString(GL_RENDERER);
        if (string == NULL)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid string");
    });
    ES3F_ADD_API_CASE(vendor, "VENDOR", {
        const GLubyte *string = glGetString(GL_VENDOR);
        if (string == NULL)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid string");
    });
    ES3F_ADD_API_CASE(version, "VERSION", {
        bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());

        const char *string          = (const char *)glGetString(GL_VERSION);
        const char *referenceString = isES ? "OpenGL ES 3." : "4.";

        if (string == NULL)
            TCU_FAIL("Got invalid string");

        if (!deStringBeginsWith(string, referenceString))
            TCU_FAIL("Got invalid string prefix");

        {
            std::string tmpString;
            char versionDelimiter;
            int glMajor             = 0;
            int glMinor             = 0;
            GLint stateVersionMinor = 0;

            std::istringstream versionStream(string);

            if (isES)
            {
                versionStream >> tmpString; // OpenGL
                versionStream >> tmpString; // ES
            }

            versionStream >> glMajor; // 3
            versionStream >> std::noskipws;
            versionStream >> versionDelimiter; // .
            versionStream >> glMinor;          // x

            if (!versionStream)
                TCU_FAIL("Got invalid string format");

            glGetIntegerv(GL_MINOR_VERSION, &stateVersionMinor);
            if (glMinor != stateVersionMinor)
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: MINOR_VERSION is " << stateVersionMinor
                                   << TestLog::EndMessage;
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid version.");
                return;
            }
        }
    });
    ES3F_ADD_API_CASE(shading_language_version, "SHADING_LANGUAGE_VERSION", {
        bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());

        const char *string          = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
        const char *referenceString = isES ? "OpenGL ES GLSL ES " : "4.";

        if (string == NULL)
            TCU_FAIL("Got invalid string");

        if (!deStringBeginsWith(string, referenceString))
            TCU_FAIL("Got invalid string prefix");

        {
            std::string tmpString;
            char versionDelimiter;
            int glslMajor        = 0;
            char glslMinorDigit1 = 0;
            char glslMinorDigit2 = 0;
            bool digitsAreValid;

            std::istringstream versionStream(string);

            if (isES)
            {
                versionStream >> tmpString; // OpenGL
                versionStream >> tmpString; // ES
                versionStream >> tmpString; // GLSL
                versionStream >> tmpString; // ES
            }

            versionStream >> glslMajor; // x
            versionStream >> std::noskipws;
            versionStream >> versionDelimiter; // .
            versionStream >> glslMinorDigit1;  // x
            versionStream >> glslMinorDigit2;  // x

            digitsAreValid =
                glslMinorDigit1 >= '0' && glslMinorDigit1 <= '9' && glslMinorDigit2 >= '0' && glslMinorDigit2 <= '9';

            if (!versionStream || !digitsAreValid)
                TCU_FAIL("Got invalid string format");
        }
    });
    ES3F_ADD_API_CASE(extensions, "EXTENSIONS", {
        const char *extensions_cstring = (const char *)glGetString(GL_EXTENSIONS);
        if (extensions_cstring == NULL)
        {
            bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());

            // GL_EXTENSIONS has been deprecated on desktop GL
            if (!isES)
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Got NULL string for deprecated enum");
                expectError(GL_INVALID_ENUM);
                return;
            }

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid string");
            return;
        }

        // split extensions_string at ' '

        std::istringstream extensionStream((std::string)(extensions_cstring));
        std::vector<std::string> extensions;

        for (;;)
        {
            std::string extension;
            if (std::getline(extensionStream, extension, ' '))
                extensions.push_back(extension);
            else
                break;
        }

        GLint numExtensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        expectError(GL_NO_ERROR);

        if (extensions.size() != (size_t)numExtensions)
        {
            m_testCtx.getLog() << TestLog::Message << "// ERROR:  NUM_EXTENSIONS is " << numExtensions << "; got "
                               << extensions.size() << " extensions" << TestLog::EndMessage;
            if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got non-consistent number of extensions");
        }

        // all in glGetStringi(GL_EXTENSIONS) in must be in glGetString

        for (int i = 0; i < numExtensions; ++i)
        {
            std::string extension((const char *)glGetStringi(GL_EXTENSIONS, i));

            if (std::find(extensions.begin(), extensions.end(), extension) == extensions.end())
            {
                m_testCtx.getLog() << TestLog::Message << "// ERROR: extension " << extension
                                   << " found with GetStringi was not found in glGetString(GL_EXTENSIONS)"
                                   << TestLog::EndMessage;
                if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Extension query methods are not consistent.");
            }
        }

        // only elements in glGetStringi(GL_EXTENSIONS) can be in glGetString

        for (int i = 0; i < numExtensions; ++i)
        {
            std::string extension((const char *)glGetStringi(GL_EXTENSIONS, i));

            std::vector<std::string>::iterator it = std::find(extensions.begin(), extensions.end(), extension);
            if (it != extensions.end())
                extensions.erase(it);
        }

        if (!extensions.empty())
        {
            for (size_t ndx = 0; ndx < extensions.size(); ++ndx)
                m_testCtx.getLog() << TestLog::Message << "// ERROR: extension \"" << extensions[ndx]
                                   << "\" found with GetString was not found with GetStringi(GL_EXTENSIONS, ind)"
                                   << TestLog::EndMessage;

            if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Extension query methods are not consistent.");
        }
    });
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
