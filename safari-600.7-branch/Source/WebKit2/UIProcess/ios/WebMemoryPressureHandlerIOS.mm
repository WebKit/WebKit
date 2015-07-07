/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WebMemoryPressureHandlerIOS.h"

#if PLATFORM(IOS)

#import "ViewSnapshotStore.h"
#import <dispatch/private.h>

namespace WebKit {

WebMemoryPressureHandler& WebMemoryPressureHandler::shared()
{
    static NeverDestroyed<WebMemoryPressureHandler> memoryPressureHandler;
    return memoryPressureHandler;
}

WebMemoryPressureHandler::WebMemoryPressureHandler()
{
    // FIXME: This should be able to share code with WebCore's MemoryPressureHandler (and be platform independent).
    // Right now it cannot because WebKit1 and WebKit2 need to be able to coexist in the UI process,
    // and you can only have one WebCore::MemoryPressureHandler.

    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MEMORYSTATUS, 0, DISPATCH_MEMORYSTATUS_PRESSURE_WARN, dispatch_get_main_queue());
    dispatch_set_context(source, this);
    dispatch_source_set_event_handler(source, ^{
        ViewSnapshotStore::shared().discardSnapshotImages();
    });
    dispatch_resume(source);
}

} // namespace WebKit

#endif
