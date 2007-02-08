/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * All rights reserved.
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

#ifndef FrameGdk_h
#define FrameGdk_h

#include "EditorClient.h"
#include "Frame.h"
#include <gdk/gdk.h>
#include "ResourceHandleClient.h"

namespace WebCore {

class Element;
class FrameGdk;
class FrameLoaderClientGdk;
class FormData;

class FrameGdk : public Frame {
public:
    FrameGdk(Page*, HTMLFrameOwnerElement*, FrameLoaderClientGdk*);
    virtual ~FrameGdk();

    // from Frame
    virtual void unfocusWindow();
    virtual void focusWindow();

    virtual Range* markedTextRange() const;

    virtual String mimeTypeForFileName(const String&) const;

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    virtual KJS::Bindings::RootObject* bindingRootObject();

    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping);
    virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const;

    virtual void print();

    // FrameGdk-only
    void handleGdkEvent(GdkEvent*);
    bool keyPress(const PlatformKeyboardEvent& keyEvent);

    void dumpRenderTree() const;

    void setExitAfterLoading(bool exitAfterLoading) { m_exitAfterLoading = exitAfterLoading; }
    bool exitAfterLoading() const { return m_exitAfterLoading; }

    void setDumpRenderTreeAfterLoading(bool dumpRenderTreeAfterLoading) { m_dumpRenderTreeAfterLoading = dumpRenderTreeAfterLoading; }
    bool dumpRenderTreeAfterLoading() const { return m_dumpRenderTreeAfterLoading; }

    void onDidFinishLoad();
private:
    bool            m_exitAfterLoading;
    bool            m_dumpRenderTreeAfterLoading;
};

inline FrameGdk* GdkFrame(Frame* frame) { return static_cast<FrameGdk*>(frame); }
inline const FrameGdk* GdkFrame(const Frame* frame) { return static_cast<const FrameGdk*>(frame); }

}

#endif
