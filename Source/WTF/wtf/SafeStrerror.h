/* 
 * Copyright (C) 2021 Red Hat Inc.
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

#pragma once

#include <wtf/Forward.h>

namespace WTF {

// This is strerror, except it is threadsafe. The problem with normal strerror is it returns a
// pointer to static storage, and it may actually modify that storage, so it can never be used in
// any multithreaded application, or any library that may be linked to a multithreaded application.
// (Why does it modify its storage? So that it can append the error number to the error string, as
// in "Unknown error n." Also, because it will localize the error message.) The standard
// alternatives are strerror_s and strerror_r, but both have problems. strerror_s is specified by
// C11, but not by C++ (as of C++20), and it is optional so glibc decided to ignore it. We can only
// rely on it to exist on Windows. Then strerror_r is even worse. First, it doesn't exist at all on
// Windows. Second, the GNU version is incompatible with the POSIX version, and it is impossible to
// use correctly unless you know which version you have. Both strerror_s and strerror_r are
// cumbersome because they force you to allocate the buffer for the result manually. It's all such a
// mess that we should deal with the complexity here rather than elsewhere in WebKit.
WTF_EXPORT_PRIVATE CString safeStrerror(int errnum);

}

using WTF::safeStrerror;
