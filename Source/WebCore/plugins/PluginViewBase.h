/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AudioHardwareListener.h"
#include "BridgeJSC.h"
#include "PlatformLayer.h"
#include "ScrollTypes.h"
#include "Widget.h"
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
typedef struct objc_object* id;
#endif

namespace JSC {
    class CallFrame;
    class JSGlobalObject;
    class JSObject;
}

namespace WebCore {

class Scrollbar;

// PluginViewBase is a widget that all plug-in views inherit from, both in Webkit and WebKit2.
// It's intended as a stopgap measure until we can merge all plug-in views into a single plug-in view.
class PluginViewBase : public Widget {
public:
    virtual PlatformLayer* platformLayer() const { return 0; }
#if PLATFORM(IOS_FAMILY)
    virtual bool willProvidePluginLayer() const { return false; }
    virtual void attachPluginLayer() { }
    virtual void detachPluginLayer() { }
#endif

    virtual JSC::JSObject* scriptObject(JSC::JSGlobalObject*) { return 0; }
    virtual void storageBlockingStateChanged() { }
    virtual bool scroll(ScrollDirection, ScrollGranularity) { return false; }

    // A plug-in can ask WebKit to handle scrollbars for it.
    virtual Scrollbar* horizontalScrollbar() { return 0; }
    virtual Scrollbar* verticalScrollbar() { return 0; }

    // FIXME: This is a hack that works around the fact that the WebKit2 PluginView isn't a ScrollableArea.
    virtual bool wantsWheelEvents() { return false; }
    virtual bool supportsKeyboardFocus() const { return false; }

    virtual bool shouldAllowNavigationFromDrags() const { return false; }

    bool isPluginViewBase() const override { return true; }

    virtual AudioHardwareActivityType audioHardwareActivity() const { return AudioHardwareActivityType::Unknown; }

    virtual void setJavaScriptPaused(bool) { }

    virtual RefPtr<JSC::Bindings::Instance> bindingInstance() { return nullptr; }
    
    virtual void willDetachRenderer() { }

#if PLATFORM(COCOA)
    virtual id accessibilityAssociatedPluginParentForElement(Element*) const { return nullptr; }
#endif
    
protected:
    explicit PluginViewBase(PlatformWidget widget = 0) : Widget(widget) { }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WIDGET(PluginViewBase, isPluginViewBase())
