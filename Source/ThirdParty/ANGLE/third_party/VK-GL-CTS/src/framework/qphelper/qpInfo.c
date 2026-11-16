/*-------------------------------------------------------------------------
 * drawElements Quality Program Helper Library
 * -------------------------------------------
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
 * \brief Version and platform info.
 *//*--------------------------------------------------------------------*/

#include "qpInfo.h"

DE_BEGIN_EXTERN_C

#if defined(DEQP_USE_RELEASE_INFO_FILE)
#include "qpReleaseInfo.inl"
#else
#define DEQP_RELEASE_NAME "unknown"
#define DEQP_RELEASE_ID 0xcafebabe
#define DEQP_RELEASE_GLSL_NAME "unknown"
#define DEQP_RELEASE_SPIRV_TOOLS_NAME "unknown"
#define DEQP_RELEASE_SPIRV_HEADERS_NAME "unknown"
#endif

const char *qpGetTargetName(void)
{
#if defined(DEQP_TARGET_NAME)
    return DEQP_TARGET_NAME;
#else
#error DEQP_TARGET_NAME is not defined!
#endif
}

const char *qpGetReleaseName(void)
{
    return DEQP_RELEASE_NAME;
}

uint32_t qpGetReleaseId(void)
{
    return DEQP_RELEASE_ID;
}

const char *qpGetReleaseGlslName(void)
{
    return DEQP_RELEASE_GLSL_NAME;
}

const char *qpGetReleaseSpirvToolsName(void)
{
    return DEQP_RELEASE_SPIRV_TOOLS_NAME;
}

const char *qpGetReleaseSpirvHeadersName(void)
{
    return DEQP_RELEASE_SPIRV_HEADERS_NAME;
}

DE_END_EXTERN_C
