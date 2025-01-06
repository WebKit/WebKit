/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/ExceptionOr.h>

namespace WebKit {

enum class FileSystemStorageError : uint8_t {
    AccessHandleActive,
    BackendNotSupported,
    FileNotFound,
    InvalidDataType,
    InvalidModification,
    InvalidName,
    InvalidState,
    MissingArgument,
    TypeMismatch,
    Unknown
};

inline WebCore::Exception convertToException(FileSystemStorageError error)
{
    switch (error) {
    case FileSystemStorageError::AccessHandleActive:
        return WebCore::Exception { WebCore::ExceptionCode::InvalidStateError, "Some AccessHandle is active"_s };
    case FileSystemStorageError::BackendNotSupported:
        return WebCore::Exception { WebCore::ExceptionCode::NotSupportedError, "Backend does not support this operation"_s };
    case FileSystemStorageError::FileNotFound:
        return WebCore::Exception { WebCore::ExceptionCode::NotFoundError };
    case FileSystemStorageError::InvalidDataType:
        return WebCore::Exception { WebCore::ExceptionCode::TypeError, "Data type is invalid"_s };
    case FileSystemStorageError::InvalidModification:
        return WebCore::Exception { WebCore::ExceptionCode::InvalidModificationError };
    case FileSystemStorageError::InvalidName:
        return WebCore::Exception { WebCore::ExceptionCode::TypeError, "Name is invalid"_s };
    case FileSystemStorageError::InvalidState:
        return WebCore::Exception { WebCore::ExceptionCode::InvalidStateError };
    case FileSystemStorageError::MissingArgument:
        return WebCore::Exception { WebCore::ExceptionCode::TypeError, "Required argument is missing"_s };
    case FileSystemStorageError::TypeMismatch:
        return WebCore::Exception { WebCore::ExceptionCode::TypeMismatchError, "File type is incompatible with handle type"_s };
    case FileSystemStorageError::Unknown:
        break;
    }

    return WebCore::Exception { WebCore::ExceptionCode::UnknownError };
}

inline WebCore::ExceptionOr<void> convertToExceptionOr(std::optional<FileSystemStorageError> error)
{
    if (!error)
        return { };

    return convertToException(*error);
}

} // namespace WebKit
