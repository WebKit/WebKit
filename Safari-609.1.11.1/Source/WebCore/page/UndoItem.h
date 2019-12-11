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

#pragma once

#include "VoidCallback.h"
#include <wtf/IsoMalloc.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class UndoManager;

class UndoItem : public RefCounted<UndoItem>, public CanMakeWeakPtr<UndoItem> {
    WTF_MAKE_ISO_ALLOCATED(UndoItem);
public:
    struct Init {
        String label;
        RefPtr<VoidCallback> undo;
        RefPtr<VoidCallback> redo;
    };

    static Ref<UndoItem> create(Init&& init)
    {
        return adoptRef(*new UndoItem(WTFMove(init)));
    }

    bool isValid() const;
    void invalidate();

    Document* document() const;

    UndoManager* undoManager() const;
    void setUndoManager(UndoManager*);

    const String& label() const { return m_label; }
    VoidCallback& undoHandler() const { return m_undoHandler.get(); }
    VoidCallback& redoHandler() const { return m_redoHandler.get(); }

private:
    UndoItem(Init&& init)
        : m_label(WTFMove(init.label))
        , m_undoHandler(init.undo.releaseNonNull())
        , m_redoHandler(init.redo.releaseNonNull())
    {
    }

    String m_label;
    Ref<VoidCallback> m_undoHandler;
    Ref<VoidCallback> m_redoHandler;
    WeakPtr<UndoManager> m_undoManager;
};

} // namespace WebCore
