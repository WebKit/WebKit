/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "PlatformSupport.h"

#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "FileMetadata.h"
#include "Page.h"
#include "WebFileInfo.h"
#include "WebFileUtilities.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebPluginContainerImpl.h"
#include "WebPluginListBuilderImpl.h"
#include "WebSandboxSupport.h"
#include "WebScreenInfo.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "platform/WebAudioBus.h"
#include "platform/WebData.h"
#include "platform/WebDragData.h"
#include "platform/WebImage.h"
#include "platform/WebKitPlatformSupport.h"
#include "platform/WebSerializedScriptValue.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"

#include "FrameView.h"
#include "IDBFactoryBackendProxy.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "PluginData.h"
#include "SharedBuffer.h"

#include "Worker.h"
#include "WorkerContextProxy.h"
#include <public/WebMimeRegistry.h>
#include <public/WebVector.h>
#include <wtf/Assertions.h>

// We are part of the WebKit implementation.
using namespace WebKit;

namespace WebCore {

// Indexed Database -----------------------------------------------------------

PassRefPtr<IDBFactoryBackendInterface> PlatformSupport::idbFactory()
{
    // There's no reason why we need to allocate a new proxy each time, but
    // there's also no strong reason not to.
    return IDBFactoryBackendProxy::create();
}

// Plugin ---------------------------------------------------------------------

bool PlatformSupport::plugins(bool refresh, Vector<PluginInfo>* results)
{
    WebPluginListBuilderImpl builder(results);
    webKitPlatformSupport()->getPluginList(refresh, &builder);
    return true;  // FIXME: There is no need for this function to return a value.
}

} // namespace WebCore
