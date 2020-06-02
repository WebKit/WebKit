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

#include "config.h"

#include "FileReaderSync.h"

#include "Blob.h"
#include "BlobURL.h"
#include "FileReaderLoader.h"
#include <JavaScriptCore/ArrayBuffer.h>

namespace WebCore {

FileReaderSync::FileReaderSync()
{
}

ExceptionOr<RefPtr<ArrayBuffer>> FileReaderSync::readAsArrayBuffer(ScriptExecutionContext& scriptExecutionContext, Blob& blob)
{
    FileReaderLoader loader(FileReaderLoader::ReadAsArrayBuffer, nullptr);
    auto result = startLoading(scriptExecutionContext, loader, blob);
    if (result.hasException())
        return result.releaseException();
    return loader.arrayBufferResult();
}

ExceptionOr<String> FileReaderSync::readAsBinaryString(ScriptExecutionContext& scriptExecutionContext, Blob& blob)
{
    FileReaderLoader loader(FileReaderLoader::ReadAsBinaryString, nullptr);
    return startLoadingString(scriptExecutionContext, loader, blob);
}

ExceptionOr<String> FileReaderSync::readAsText(ScriptExecutionContext& scriptExecutionContext, Blob& blob, const String& encoding)
{
    FileReaderLoader loader(FileReaderLoader::ReadAsText, nullptr);
    loader.setEncoding(encoding);
    return startLoadingString(scriptExecutionContext, loader, blob);
}

ExceptionOr<String> FileReaderSync::readAsDataURL(ScriptExecutionContext& scriptExecutionContext, Blob& blob)
{
    FileReaderLoader loader(FileReaderLoader::ReadAsDataURL, nullptr);
    loader.setDataType(blob.type());
    return startLoadingString(scriptExecutionContext, loader, blob);
}

static ExceptionOr<void> errorCodeToException(FileError::ErrorCode errorCode)
{
    switch (errorCode) {
    case FileError::OK:
        return { };
    case FileError::NOT_FOUND_ERR:
        return Exception { NotFoundError };
    case FileError::SECURITY_ERR:
        return Exception { SecurityError };
    case FileError::ABORT_ERR:
        return Exception { AbortError };
    case FileError::NOT_READABLE_ERR:
        return Exception { NotReadableError };
    case FileError::ENCODING_ERR:
        return Exception { EncodingError };
    case FileError::NO_MODIFICATION_ALLOWED_ERR:
        return Exception { NoModificationAllowedError };
    case FileError::INVALID_STATE_ERR:
        return Exception { InvalidStateError };
    case FileError::SYNTAX_ERR:
        return Exception { SyntaxError };
    case FileError::INVALID_MODIFICATION_ERR:
        return Exception { InvalidModificationError };
    case FileError::QUOTA_EXCEEDED_ERR:
        return Exception { QuotaExceededError };
    case FileError::TYPE_MISMATCH_ERR:
        return Exception { TypeMismatchError };
    case FileError::PATH_EXISTS_ERR:
        return Exception { NoModificationAllowedError };
    }
    return Exception { UnknownError };
}

ExceptionOr<void> FileReaderSync::startLoading(ScriptExecutionContext& scriptExecutionContext, FileReaderLoader& loader, Blob& blob)
{
    loader.start(&scriptExecutionContext, blob);
    return errorCodeToException(loader.errorCode());
}

ExceptionOr<String> FileReaderSync::startLoadingString(ScriptExecutionContext& scriptExecutionContext, FileReaderLoader& loader, Blob& blob)
{
    auto result = startLoading(scriptExecutionContext, loader, blob);
    if (result.hasException())
        return result.releaseException();
    return loader.stringResult();
}

} // namespace WebCore
