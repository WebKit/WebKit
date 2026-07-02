/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief SHA1 hash functions
 *//*--------------------------------------------------------------------*/

#include "deSha1.hpp"

namespace de
{

Sha1 Sha1::parse(const std::string &str)
{
    deSha1 hash;

    DE_CHECK_RUNTIME_ERR_MSG(str.size() >= 40, "Failed to parse SHA1. String is too short.");
    DE_CHECK_RUNTIME_ERR_MSG(deSha1_parse(&hash, str.c_str()), "Failed to parse SHA1. Invalid characters..");

    return Sha1(hash);
}

Sha1 Sha1::compute(size_t size, const void *data)
{
    deSha1 hash;

    deSha1_compute(&hash, size, data);
    return Sha1(hash);
}

Sha1Stream::Sha1Stream(void)
{
    deSha1Stream_init(&m_stream);
}

void Sha1Stream::process(size_t size, const void *data)
{
    deSha1Stream_process(&m_stream, size, data);
}

Sha1 Sha1Stream::finalize(void)
{
    deSha1 hash;
    deSha1Stream_finalize(&m_stream, &hash);

    return Sha1(hash);
}

} // namespace de
