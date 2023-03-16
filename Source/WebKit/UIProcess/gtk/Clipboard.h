/*
 * Copyright (C) 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>

#if USE(GTK4)
typedef struct _GdkClipboard GdkClipboard;
#else
typedef struct _GtkClipboard GtkClipboard;
#endif

namespace WebCore {
class SelectionData;
class SharedBuffer;
}

namespace WebKit {

class WebFrameProxy;

class Clipboard {
    WTF_MAKE_NONCOPYABLE(Clipboard); WTF_MAKE_FAST_ALLOCATED;
public:
    static Clipboard& get(const String& name);

    enum class Type { Clipboard, Primary };
    explicit Clipboard(Type);
    ~Clipboard();

    Type type() const;
    void formats(CompletionHandler<void(Vector<String>&&)>&&);
    void readText(CompletionHandler<void(String&&)>&&);
    void readFilePaths(CompletionHandler<void(Vector<String>&&)>&&);
    void readURL(CompletionHandler<void(String&& url, String&& title)>&&);
    void readBuffer(const char*, CompletionHandler<void(Ref<WebCore::SharedBuffer>&&)>&&);
    void write(WebCore::SelectionData&&, CompletionHandler<void(int64_t)>&&);
    void clear();

    int64_t changeCount() const { return m_changeCount; }

private:
#if USE(GTK4)
    GdkClipboard* m_clipboard { nullptr };
#else
    GtkClipboard* m_clipboard { nullptr };
    WebFrameProxy* m_frameWritingToClipboard { nullptr };
#endif
    int64_t m_changeCount { 0 };
};

} // namespace WebKit
