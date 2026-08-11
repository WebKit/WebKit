#ifndef _TES3TESTPACKAGE_HPP
#define _TES3TESTPACKAGE_HPP
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
 * \brief OpenGL ES 3.0 Test Package
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestPackage.hpp"
#include "tes3Context.hpp"
#include "tcuResource.hpp"
#include "deSharedPtr.hpp"

namespace tcu
{
class WaiverUtil;
}

namespace deqp
{
namespace gles3
{

class TestPackage : public tcu::TestPackage
{
public:
    TestPackage(tcu::TestContext &testCtx);
    virtual ~TestPackage(void);

    virtual void init(void);
    virtual void deinit(void);

    tcu::TestCaseExecutor *createExecutor(void) const;

    tcu::Archive *getArchive(void)
    {
        return &m_archive;
    }
    Context *getContext(void)
    {
        return m_context;
    }

private:
    tcu::ResourcePrefix m_archive;
    Context *m_context;
    de::SharedPtr<tcu::WaiverUtil> m_waiverMechanism;
};

} // namespace gles3
} // namespace deqp

#endif // _TES3TESTPACKAGE_HPP
