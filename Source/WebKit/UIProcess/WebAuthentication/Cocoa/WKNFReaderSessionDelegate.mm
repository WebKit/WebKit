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

#import "config.h"
#import "WKNFReaderSessionDelegate.h"

#if ENABLE(WEB_AUTHN) && HAVE(NEAR_FIELD)

#import "NfcConnection.h"
#import <wtf/RunLoop.h>
#import <wtf/WeakPtr.h>

#import "NearFieldSoftLink.h"

@implementation WKNFReaderSessionDelegate {
    WeakPtr<WebKit::NfcConnection> _connection;
}

- (instancetype)initWithConnection:(WebKit::NfcConnection&)connection
{
    if ((self = [super init]))
        _connection = makeWeakPtr(connection);
    return self;
}

// Executed in a different thread.
- (void)readerSession:(NFReaderSession*)theSession didDetectTags:(NSArray<NFTag *> *)tags
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([connection = _connection, tags = retainPtr(tags)] {
        if (!connection)
            return;
        connection->didDetectTags(tags.get());
    });
}

@end

#endif // ENABLE(WEB_AUTHN) && HAVE(NEAR_FIELD)
