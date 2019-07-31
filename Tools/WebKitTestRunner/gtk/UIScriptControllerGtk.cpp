/*
 * Copyright (C) 2019 Alexander Mikhaylenko <exalm7659@gmail.com>
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

#include "config.h"
#include "UIScriptControllerGtk.h"

#include "PlatformWebView.h"
#include "TestController.h"
#include <WebKit/WKViewPrivate.h>
#include <gtk/gtk.h>

namespace WTR {

Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptControllerGtk(context));
}

void UIScriptControllerGtk::beginBackSwipe(JSValueRef callback)
{
    auto* webView = TestController::singleton().mainWebView()->platformView();

    WKViewBeginBackSwipeForTesting(webView);
}

void UIScriptControllerGtk::completeBackSwipe(JSValueRef callback)
{
    auto* webView = TestController::singleton().mainWebView()->platformView();

    WKViewCompleteBackSwipeForTesting(webView);
}

bool UIScriptControllerGtk::isShowingDataListSuggestions() const
{
    auto* webView = TestController::singleton().mainWebView()->platformView();
    if (auto* popup = g_object_get_data(G_OBJECT(webView), "wk-datalist-popup"))
        return gtk_widget_get_mapped(GTK_WIDGET(popup));
    return false;
}

} // namespace WTR
