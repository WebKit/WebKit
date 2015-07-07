/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#if ENABLE(MEDIA_CONTROLS_SCRIPT)

#import "QuickTimePluginReplacement.h"

#import "Event.h"
#import "HTMLPlugInElement.h"
#import "HTMLVideoElement.h"
#import "JSDOMBinding.h"
#import "JSDOMGlobalObject.h"
#import "JSHTMLVideoElement.h"
#import "JSQuickTimePluginReplacement.h"
#import "Logging.h"
#import "MainFrame.h"
#import "Page.h"
#import "RenderElement.h"
#import "ScriptController.h"
#import "ScriptSourceCode.h"
#import "SoftLinking.h"
#import "UserAgentScripts.h"
#import <objc/runtime.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/NSString.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <JavaScriptCore/APICast.h>
#import <wtf/text/Base64.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(CoreMedia)
SOFT_LINK(CoreMedia, CMTimeCopyAsDictionary, CFDictionaryRef, (CMTime time, CFAllocatorRef allocator), (time, allocator))

typedef AVMetadataItem AVMetadataItemType;
SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVMetadataItem)
#define AVMetadataItem getAVMetadataItemClass()

namespace WebCore {

#if PLATFORM(IOS)
static JSValue *jsValueWithValueInContext(id, JSContext *);
static JSValue *jsValueWithAVMetadataItemInContext(AVMetadataItemType *, JSContext *);
#endif

static String quickTimePluginReplacementScript()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(String, script, (QuickTimePluginReplacementJavaScript, sizeof(QuickTimePluginReplacementJavaScript)));
    return script;
}

void QuickTimePluginReplacement::registerPluginReplacement(PluginReplacementRegistrar registrar)
{
    registrar(ReplacementPlugin(create, supportsMimeType, supportsFileExtension, supportsURL));
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
    DEPRECATED_DEFINE_STATIC_LOCAL(HashSet<String>, typeHash, ());
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
    DEPRECATED_DEFINE_STATIC_LOCAL(HashSet<String>, extensionHash, ());
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
        return m_mediaElement->createElementRenderer(WTF::move(style));

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
    JSDOMGlobalObject* globalObject = JSC::jsCast<JSDOMGlobalObject*>(scriptController.globalObject(world));
    JSC::ExecState* exec = globalObject->globalExec();
    JSC::JSLockHolder lock(exec);
    
    JSC::JSValue replacementFunction = globalObject->get(exec, JSC::Identifier(exec, "createPluginReplacement"));
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
    JSDOMGlobalObject* globalObject = JSC::jsCast<JSDOMGlobalObject*>(scriptController.globalObject(world));
    JSC::ExecState* exec = globalObject->globalExec();
    JSC::JSLockHolder lock(exec);
    
    // Lookup the "createPluginReplacement" function.
    JSC::JSValue replacementFunction = globalObject->get(exec, JSC::Identifier(exec, "createPluginReplacement"));
    if (replacementFunction.isUndefinedOrNull())
        return false;
    JSC::JSObject* replacementObject = replacementFunction.toObject(exec);
    JSC::CallData callData;
    JSC::CallType callType = replacementObject->methodTable()->getCallData(replacementObject, callData);
    if (callType == JSC::CallTypeNone)
        return false;

    JSC::MarkedArgumentBuffer argList;
    argList.append(toJS(exec, globalObject, root));
    argList.append(toJS(exec, globalObject, m_parentElement));
    argList.append(toJS(exec, globalObject, this));
    argList.append(toJS<String>(exec, globalObject, m_names));
    argList.append(toJS<String>(exec, globalObject, m_values));
    JSC::JSValue replacement = call(exec, replacementObject, callType, callData, globalObject, argList);
    if (exec->hadException()) {
        exec->clearException();
        return false;
    }

    // Get the <video> created to replace the plug-in.
    JSC::JSValue value = replacement.get(exec, JSC::Identifier(exec, "video"));
    if (!exec->hadException() && !value.isUndefinedOrNull())
        m_mediaElement = toHTMLVideoElement(value);

    if (!m_mediaElement) {
        LOG(Plugins, "%p - Failed to find <video> element created by QuickTime plugin replacement script.", this);
        exec->clearException();
        return false;
    }

    // Get the scripting interface.
    value = replacement.get(exec, JSC::Identifier(exec, "scriptObject"));
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

#if PLATFORM(IOS)
static JSValue *jsValueWithDataInContext(NSData *data, const String& mimeType, JSContext *context)
{
    Vector<char> base64Data;
    base64Encode([data bytes], [data length], base64Data);

    String data64;
    if (!mimeType.isEmpty())
        data64 = "data:" + mimeType + ";base64," + base64Data;
    else
        data64 = "data:text/plain;base64," + base64Data;

    return [JSValue valueWithObject:(id)data64.createCFString().get() inContext:context];
}

static JSValue *jsValueWithArrayInContext(NSArray *array, JSContext *context)
{
    JSValueRef exception = 0;
    JSValue *result = [JSValue valueWithNewArrayInContext:context];
    JSObjectRef resultObject = JSValueToObject([context JSGlobalContextRef], [result JSValueRef], &exception);
    if (exception)
        return [JSValue valueWithUndefinedInContext:context];

    NSUInteger count = [array count];
    for (NSUInteger i = 0; i < count; ++i) {
        JSValue *value = jsValueWithValueInContext([array objectAtIndex:i], context);
        if (!value)
            continue;

        JSObjectSetPropertyAtIndex([context JSGlobalContextRef], resultObject, (unsigned)i, [value JSValueRef], &exception);
        if (exception)
            continue;
    }

    return result;
}


static JSValue *jsValueWithDictionaryInContext(NSDictionary *dictionary, JSContext *context)
{
    JSValueRef exception = 0;
    JSValue *result = [JSValue valueWithNewObjectInContext:context];
    JSObjectRef resultObject = JSValueToObject([context JSGlobalContextRef], [result JSValueRef], &exception);
    if (exception)
        return [JSValue valueWithUndefinedInContext:context];

    for (id key in [dictionary keyEnumerator]) {
        if (![key isKindOfClass:[NSString class]])
            continue;

        JSValue *value = jsValueWithValueInContext([dictionary objectForKey:key], context);
        if (!value)
            continue;

        JSStringRef name = JSStringCreateWithCFString((CFStringRef)key);
        JSObjectSetProperty([context JSGlobalContextRef], resultObject, name, [value JSValueRef], 0, &exception);
        if (exception)
            continue;
    }

    return result;
}

static JSValue *jsValueWithValueInContext(id value, JSContext *context)
{
    if ([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSNumber class]])
        return [JSValue valueWithObject:value inContext:context];
    else if ([value isKindOfClass:[NSLocale class]])
        return [JSValue valueWithObject:[value localeIdentifier] inContext:context];
    else if ([value isKindOfClass:[NSDictionary class]])
        return jsValueWithDictionaryInContext(value, context);
    else if ([value isKindOfClass:[NSArray class]])
        return jsValueWithArrayInContext(value, context);
    else if ([value isKindOfClass:[NSData class]])
        return jsValueWithDataInContext(value, emptyString(), context);
    else if ([value isKindOfClass:[AVMetadataItem class]])
        return jsValueWithAVMetadataItemInContext(value, context);

    return nil;
}

static JSValue *jsValueWithAVMetadataItemInContext(AVMetadataItemType *item, JSContext *context)
{
    NSMutableDictionary* dictionary = [NSMutableDictionary dictionaryWithDictionary:[item extraAttributes]];

    if (item.keySpace)
        [dictionary setObject:item.keySpace forKey:@"keyspace"];

    if (item.key)
        [dictionary setObject:item.key forKey:@"key"];

    if (item.locale)
        [dictionary setObject:item.locale forKey:@"locale"];

    if (CMTIME_IS_VALID(item.time)) {
        CFDictionaryRef timeDict = CMTimeCopyAsDictionary(item.time, kCFAllocatorDefault);

        if (timeDict) {
            [dictionary setObject:(id)timeDict forKey:@"timestamp"];
            CFRelease(timeDict);
        }
    }
    
    if (item.value) {
        id value = item.value;
        NSString *mimeType = [[item extraAttributes] objectForKey:@"MIMEtype"];
        if ([value isKindOfClass:[NSData class]] && mimeType) {
            Vector<char> base64Data;
            base64Encode([value bytes], [value length], base64Data);
            String data64 = "data:" + String(mimeType) + ";base64," + base64Data;
            [dictionary setObject:(id)data64.createCFString().get() forKey:@"value"];
        } else
            [dictionary setObject:value forKey:@"value"];
    }

    return jsValueWithDictionaryInContext(dictionary, context);
}
#endif

JSC::JSValue JSQuickTimePluginReplacement::timedMetaData(JSC::ExecState* exec) const
{
#if PLATFORM(IOS)
    HTMLVideoElement* parent = impl().parentElement();
    if (!parent || !parent->player())
        return JSC::jsNull();

    Frame* frame = parent->document().frame();
    if (!frame)
        return JSC::jsNull();

    NSArray *metaData = parent->player()->timedMetadata();
    if (!metaData)
        return JSC::jsNull();

    JSContext *jsContext = frame->script().javaScriptContext();
    JSValue *metaDataValue = jsValueWithValueInContext(metaData, jsContext);
    
    return toJS(exec, [metaDataValue JSValueRef]);
#else
    UNUSED_PARAM(exec);
    return JSC::jsNull();
#endif
}

JSC::JSValue JSQuickTimePluginReplacement::accessLog(JSC::ExecState* exec) const
{
#if PLATFORM(IOS)
    HTMLVideoElement* parent = impl().parentElement();
    if (!parent || !parent->player())
        return JSC::jsNull();

    Frame* frame = parent->document().frame();
    if (!frame)
        return JSC::jsNull();

    JSValue *dictionary = [JSValue valueWithNewObjectInContext:frame->script().javaScriptContext()];
    String accessLogString = parent->player()->accessLog();
    [dictionary setValue:static_cast<NSString *>(accessLogString) forProperty:(NSString *)CFSTR("extendedLog")];

    return toJS(exec, [dictionary JSValueRef]);
#else
    UNUSED_PARAM(exec);
    return JSC::jsNull();
#endif
}

JSC::JSValue JSQuickTimePluginReplacement::errorLog(JSC::ExecState* exec) const
{
#if PLATFORM(IOS)
    HTMLVideoElement* parent = impl().parentElement();
    if (!parent || !parent->player())
        return JSC::jsNull();

    Frame* frame = parent->document().frame();
    if (!frame)
        return JSC::jsNull();

    JSValue *dictionary = [JSValue valueWithNewObjectInContext:frame->script().javaScriptContext()];
    String errorLogString = parent->player()->errorLog();
    [dictionary setValue:static_cast<NSString *>(errorLogString) forProperty:(NSString *)CFSTR("extendedLog")];

    return toJS(exec, [dictionary JSValueRef]);
#else
    UNUSED_PARAM(exec);
    return JSC::jsNull();
#endif
}

}

#endif
