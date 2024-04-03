/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionDataRecord.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "_WKWebExtensionDataRecordInternal.h"

namespace WebKit {

NSArray *WebExtensionDataRecord::errors()
{
    return [m_errors copy] ?: @[ ];
}

void WebExtensionDataRecord::addError(NSString *debugDescription, WebExtensionDataType type)
{
    if (!m_errors)
        m_errors = [[NSMutableArray alloc] init];

    switch (type) {
    case WebExtensionDataType::Local:
        [m_errors.get() addObject:createDataRecordError(_WKWebExtensionDataRecordErrorLocalStorageFailed, debugDescription)];
        break;
    case WebExtensionDataType::Session:
        [m_errors.get() addObject:createDataRecordError(_WKWebExtensionDataRecordErrorSessionStorageFailed, debugDescription)];
        break;
    case WebExtensionDataType::Sync:
        [m_errors.get() addObject:createDataRecordError(_WKWebExtensionDataRecordErrorSyncStorageFailed, debugDescription)];
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
