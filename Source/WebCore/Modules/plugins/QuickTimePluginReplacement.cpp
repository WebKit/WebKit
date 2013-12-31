/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(MEDIA_CONTROLS_SCRIPT)

#include "QuickTimePluginReplacement.h"

#include "Event.h"
#include "HTMLPlugInElement.h"
#include "HTMLVideoElement.h"
#include "JSDOMBinding.h"
#include "JSDOMGlobalObject.h"
#include "JSHTMLVideoElement.h"
#include "JSQuickTimePluginReplacement.h"
#include "Logging.h"
#include "MainFrame.h"
#include "Page.h"
#include "RenderElement.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "UserAgentScripts.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSCJSValueInlines.h>

using namespace JSC;

namespace WebCore {

static String quickTimePluginReplacementScript()
{
    DEFINE_STATIC_LOCAL(String, script, (QuickTimePluginReplacementJavaScript, sizeof(QuickTimePluginReplacementJavaScript)));
    return script;
}

void QuickTimePluginReplacement::registerPluginReplacement(PluginReplacementRegistrar registrar)
{
    registrar(ReplacementPlugin(create, supportsMimeType, supportsFileExtension));
}

PassRefPtr<PluginReplacement> QuickTimePluginReplacement::create(HTMLPlugInElement& plugin, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    return adoptRef(new QuickTimePluginReplacement(plugin, paramNames, paramValues));
}

bool QuickTimePluginReplacement::supportsMimeType(const String& mimeType)
{
    static const char* types[] = {
        "application/vnd.apple.mpegurl", "application/x-mpegurl", "audio/3gpp", "audio/3gpp2", "audio/aac", "audio/aiff",
        "audio/amr", "audio/basic", "audio/mp3", "audio/mp4", "audio/mpeg", "audio/mpeg3", "audio/mpegurl", "audio/scpls",
        "audio/wav", "audio/x-aac", "audio/x-aiff", "audio/x-caf", "audio/x-m4a", "audio/x-m4b", "audio/x-m4p",
        "audio/x-m4r", "audio/x-mp3", "audio/x-mpeg", "audio/x-mpeg3", "audio/x-mpegurl", "audio/x-scpls", "audio/x-wav",
        "video/3gpp", "video/3gpp2", "video/mp4", "video/quicktime", "video/x-m4v"
    };
    DEFINE_STATIC_LOCAL(HashSet<String>, typeHash, ());
    if (!typeHash.size()) {
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(types); ++i)
            typeHash.add(types[i]);
    }

    return typeHash.contains(mimeType);
}

bool QuickTimePluginReplacement::supportsFileExtension(const String& extension)
{
    static const char* extensions[] = {
        "3g2", "3gp", "3gp2", "3gpp", "aac", "adts", "aif", "aifc", "aiff", "AMR", "au", "bwf", "caf", "cdda", "m3u",
        "m3u8", "m4a", "m4b", "m4p", "m4r", "m4v", "mov", "mp3", "mp3", "mp4", "mpeg", "mpg", "mqv", "pls", "qt",
        "snd", "swa", "ts", "ulw", "wav"
    };
    DEFINE_STATIC_LOCAL(HashSet<String>, extensionHash, ());
    if (!extensionHash.size()) {
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(extensions); ++i)
            extensionHash.add(extensions[i]);
    }

    return extensionHash.contains(extension);
}

QuickTimePluginReplacement::QuickTimePluginReplacement(HTMLPlugInElement& plugin, const Vector<String>& paramNames, const Vector<String>& paramValues)
    :PluginReplacement()
    , m_parentElement(&plugin)
    , m_names(paramNames)
    , m_values(paramValues)
    , m_scriptObject(nullptr)
{
}

QuickTimePluginReplacement::~QuickTimePluginReplacement()
{
    m_parentElement = nullptr;
    m_scriptObject = nullptr;
    m_mediaElement = nullptr;
}

RenderPtr<RenderElement> QuickTimePluginReplacement::createElementRenderer(HTMLPlugInElement& plugin, PassRef<RenderStyle> style)
{
    ASSERT_UNUSED(plugin, m_parentElement == &plugin);

    if (m_mediaElement)
        return m_mediaElement->createElementRenderer(std::move(style));

    return nullptr;
}

DOMWrapperWorld& QuickTimePluginReplacement::isolatedWorld()
{
    static DOMWrapperWorld& isolatedWorld = *DOMWrapperWorld::create(JSDOMWindow::commonVM()).leakRef();
    return isolatedWorld;
}

bool QuickTimePluginReplacement::ensureReplacementScriptInjected()
{
    Page* page = m_parentElement->document().page();
    if (!page)
        return false;
    
    DOMWrapperWorld& world = isolatedWorld();
    ScriptController& scriptController = page->mainFrame().script();
    JSDOMGlobalObject* globalObject = jsCast<JSDOMGlobalObject*>(scriptController.globalObject(world));
    ExecState* exec = globalObject->globalExec();
    
    JSValue replacementFunction = globalObject->get(exec, Identifier(exec, "createPluginReplacement"));
    if (replacementFunction.isFunction())
        return true;
    
    scriptController.evaluateInWorld(ScriptSourceCode(quickTimePluginReplacementScript()), world);
    if (exec->hadException()) {
        LOG(Plugins, "%p - Exception when evaluating QuickTime plugin replacement script", this);
        exec->clearException();
        return false;
    }
    
    return true;
}

bool QuickTimePluginReplacement::installReplacement(ShadowRoot* root)
{
    Page* page = m_parentElement->document().page();

    if (!ensureReplacementScriptInjected())
        return false;

    DOMWrapperWorld& world = isolatedWorld();
    ScriptController& scriptController = page->mainFrame().script();
    JSDOMGlobalObject* globalObject = jsCast<JSDOMGlobalObject*>(scriptController.globalObject(world));
    ExecState* exec = globalObject->globalExec();
    JSLockHolder lock(exec);
    
    // Lookup the "createPluginReplacement" function.
    JSValue replacementFunction = globalObject->get(exec, Identifier(exec, "createPluginReplacement"));
    if (replacementFunction.isUndefinedOrNull())
        return false;
    JSObject* replacementObject = replacementFunction.toObject(exec);
    CallData callData;
    CallType callType = replacementObject->methodTable()->getCallData(replacementObject, callData);
    if (callType == CallTypeNone)
        return false;

    MarkedArgumentBuffer argList;
    argList.append(toJS(exec, globalObject, root));
    argList.append(toJS(exec, globalObject, m_parentElement));
    argList.append(toJS(exec, globalObject, this));
    argList.append(toJS<String>(exec, globalObject, m_names));
    argList.append(toJS<String>(exec, globalObject, m_values));
    JSValue replacement = call(exec, replacementObject, callType, callData, globalObject, argList);
    if (exec->hadException()) {
        exec->clearException();
        return false;
    }

    // Get the <video> created to replace the plug-in.
    JSValue value = replacement.get(exec, Identifier(exec, "video"));
    if (!exec->hadException() && !value.isUndefinedOrNull())
        m_mediaElement = toHTMLVideoElement(value);

    if (!m_mediaElement) {
        LOG(Plugins, "%p - Failed to find <video> element created by QuickTime plugin replacement script.", this);
        exec->clearException();
        return false;
    }

    // Get the scripting interface.
    value = replacement.get(exec, Identifier(exec, "scriptObject"));
    if (!exec->hadException() && !value.isUndefinedOrNull())
        m_scriptObject = value.toObject(exec);

    if (!m_scriptObject) {
        LOG(Plugins, "%p - Failed to find script object created by QuickTime plugin replacement.", this);
        exec->clearException();
        return false;
    }

    return true;
}

unsigned long long QuickTimePluginReplacement::movieSize() const
{
    if (m_mediaElement)
        return m_mediaElement->fileSize();

    return 0;
}

void QuickTimePluginReplacement::postEvent(const String& eventName)
{
    Ref<HTMLPlugInElement> protect(*m_parentElement);
    RefPtr<Event> event = Event::create(eventName, false, true);
    m_parentElement->dispatchEvent(event.get());
}

}
#endif
