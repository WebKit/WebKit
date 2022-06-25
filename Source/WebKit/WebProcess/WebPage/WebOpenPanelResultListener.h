/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class FileChooser;
class Icon;
}

namespace WebKit {

class WebPage;

class WebOpenPanelResultListener : public RefCounted<WebOpenPanelResultListener> {
public:
    static Ref<WebOpenPanelResultListener> create(WebPage&, Ref<WebCore::FileChooser>&&);
    ~WebOpenPanelResultListener();

    void disconnectFromPage() { m_page = 0; }
    void didChooseFiles(const Vector<String>& files, const Vector<String>& replacementFiles);
#if PLATFORM(IOS_FAMILY)
    void didChooseFilesWithDisplayStringAndIcon(const Vector<String>&, const String& displayString, WebCore::Icon*);
#endif

private:
    WebOpenPanelResultListener(WebPage&, Ref<WebCore::FileChooser>&&);

    WebPage* m_page;
    Ref<WebCore::FileChooser> m_fileChooser;
};

} // namespace WebKit
