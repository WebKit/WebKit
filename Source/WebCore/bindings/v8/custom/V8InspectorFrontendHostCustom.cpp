/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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
#if ENABLE(INSPECTOR)
#include "V8InspectorFrontendHost.h"

#include "HistogramSupport.h"
#include "InspectorController.h"
#include "InspectorFrontendClient.h"
#include "InspectorFrontendHost.h"
#include <wtf/text/WTFString.h>

#include "V8Binding.h"
#include "V8MouseEvent.h"

namespace WebCore {

v8::Handle<v8::Value> V8InspectorFrontendHost::platformMethodCustom(const v8::Arguments& args)
{
#if defined(OS_MACOSX)
    return v8::String::NewSymbol("mac");
#elif defined(OS_LINUX)
    return v8::String::NewSymbol("linux");
#elif defined(OS_FREEBSD)
    return v8::String::NewSymbol("freebsd");
#elif defined(OS_OPENBSD)
    return v8::String::NewSymbol("openbsd");
#elif defined(OS_SOLARIS)
    return v8::String::NewSymbol("solaris");
#elif defined(OS_WIN)
    return v8::String::NewSymbol("windows");
#else
    return v8::String::NewSymbol("unknown");
#endif
}

v8::Handle<v8::Value> V8InspectorFrontendHost::portMethodCustom(const v8::Arguments&)
{
    return v8::Undefined();
}

static void populateContextMenuItems(v8::Local<v8::Array>& itemArray, ContextMenu& menu)
{
    for (size_t i = 0; i < itemArray->Length(); ++i) {
        v8::Local<v8::Object> item = v8::Local<v8::Object>::Cast(itemArray->Get(i));
        v8::Local<v8::Value> type = item->Get(v8::String::NewSymbol("type"));
        v8::Local<v8::Value> id = item->Get(v8::String::NewSymbol("id"));
        v8::Local<v8::Value> label = item->Get(v8::String::NewSymbol("label"));
        v8::Local<v8::Value> enabled = item->Get(v8::String::NewSymbol("enabled"));
        v8::Local<v8::Value> checked = item->Get(v8::String::NewSymbol("checked"));
        v8::Local<v8::Value> subItems = item->Get(v8::String::NewSymbol("subItems"));
        if (!type->IsString())
            continue;
        String typeString = toWebCoreStringWithNullCheck(type);
        if (typeString == "separator") {
            ContextMenuItem item(ContextMenuItem(SeparatorType,
                                 ContextMenuItemCustomTagNoAction,
                                 String()));
            menu.appendItem(item);
        } else if (typeString == "subMenu" && subItems->IsArray()) {
            ContextMenu subMenu;
            v8::Local<v8::Array> subItemsArray = v8::Local<v8::Array>::Cast(subItems);
            populateContextMenuItems(subItemsArray, subMenu);
            ContextMenuItem item(SubmenuType,
                                 ContextMenuItemCustomTagNoAction,
                                 toWebCoreStringWithNullCheck(label),
                                 &subMenu);
            menu.appendItem(item);
        } else {
            ContextMenuAction typedId = static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + id->ToInt32()->Value());
            ContextMenuItem menuItem((typeString == "checkbox" ? CheckableActionType : ActionType), typedId, toWebCoreStringWithNullCheck(label));
            if (checked->IsBoolean())
                menuItem.setChecked(checked->ToBoolean()->Value());
            if (enabled->IsBoolean())
                menuItem.setEnabled(enabled->ToBoolean()->Value());
            menu.appendItem(menuItem);
        }
    }
}

v8::Handle<v8::Value> V8InspectorFrontendHost::showContextMenuMethodCustom(const v8::Arguments& args)
{
    if (args.Length() < 2)
        return v8::Undefined();

    v8::Local<v8::Object> eventWrapper = v8::Local<v8::Object>::Cast(args[0]);
    if (!V8MouseEvent::info.equals(toWrapperTypeInfo(eventWrapper)))
        return v8::Undefined();

    Event* event = V8Event::toNative(eventWrapper);
    if (!args[1]->IsArray())
        return v8::Undefined();

    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(args[1]);
    ContextMenu menu;
    populateContextMenuItems(array, menu);

    InspectorFrontendHost* frontendHost = V8InspectorFrontendHost::toNative(args.Holder());
#if !USE(CROSS_PLATFORM_CONTEXT_MENUS)
    Vector<ContextMenuItem> items = contextMenuItemVector(menu.platformDescription());
#else
    Vector<ContextMenuItem> items = menu.items();
#endif
    frontendHost->showContextMenu(event, items);

    return v8::Undefined();
}

static v8::Handle<v8::Value> histogramEnumeration(const char* name, const v8::Arguments& args, int boundaryValue)
{
    if (args.Length() < 1 || !args[0]->IsInt32())
        return v8::Undefined();

    int sample = args[0]->ToInt32()->Value();
    if (sample < boundaryValue)
        HistogramSupport::histogramEnumeration(name, sample, boundaryValue);

    return v8::Undefined();
}

v8::Handle<v8::Value> V8InspectorFrontendHost::recordActionTakenMethodCustom(const v8::Arguments& args)
{
    return histogramEnumeration("DevTools.ActionTaken", args, 100);
}

v8::Handle<v8::Value> V8InspectorFrontendHost::recordPanelShownMethodCustom(const v8::Arguments& args)
{
    return histogramEnumeration("DevTools.PanelShown", args, 20);
}

v8::Handle<v8::Value> V8InspectorFrontendHost::recordSettingChangedMethodCustom(const v8::Arguments& args)
{
    return histogramEnumeration("DevTools.SettingChanged", args, 100);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
