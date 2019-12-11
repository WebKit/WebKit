/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BytecodeCacheError.h"

namespace JSC {

bool BytecodeCacheError::StandardError::isValid() const
{
    return true;
}

String BytecodeCacheError::StandardError::message() const
{
    return strerror(m_errno);
}

bool BytecodeCacheError::WriteError::isValid() const
{
    return true;
}

String BytecodeCacheError::WriteError::message() const
{
    return makeString("Could not write the full cache file to disk. Only wrote ", String::number(m_written), " of the expected ", String::number(m_expected), " bytes.");
}

BytecodeCacheError& BytecodeCacheError::operator=(const ParserError& error)
{
    m_error = error;
    return *this;
}

BytecodeCacheError& BytecodeCacheError::operator=(const StandardError& error)
{
    m_error = error;
    return *this;
}

BytecodeCacheError& BytecodeCacheError::operator=(const WriteError& error)
{
    m_error = error;
    return *this;
}

bool BytecodeCacheError::isValid() const
{
    return WTF::switchOn(m_error, [](const auto& error) {
        return error.isValid();
    });
}

String BytecodeCacheError::message() const
{
    return WTF::switchOn(m_error, [](const auto& error) {
        return error.message();
    });
}

} // namespace JSC
