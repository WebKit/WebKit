/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include "FileError.h"

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

class Blob;
class FileReaderLoader;
class ScriptExecutionContext;

class FileReaderSync : public RefCounted<FileReaderSync> {
public:
    static Ref<FileReaderSync> create()
    {
        return adoptRef(*new FileReaderSync);
    }

    ExceptionOr<RefPtr<JSC::ArrayBuffer>> readAsArrayBuffer(ScriptExecutionContext&, Blob&);
    ExceptionOr<String> readAsBinaryString(ScriptExecutionContext&, Blob&);
    ExceptionOr<String> readAsText(ScriptExecutionContext&, Blob&, const String& encoding);
    ExceptionOr<String> readAsDataURL(ScriptExecutionContext&, Blob&);

    static ExceptionOr<void> errorCodeToException(FileError::ErrorCode);

private:
    FileReaderSync();

    ExceptionOr<void> startLoading(ScriptExecutionContext&, FileReaderLoader&, Blob&);
    ExceptionOr<String> startLoadingString(ScriptExecutionContext&, FileReaderLoader&, Blob&);
};

} // namespace WebCore
