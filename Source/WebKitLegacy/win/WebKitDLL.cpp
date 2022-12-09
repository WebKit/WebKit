/*
 * Copyright (C) 2006-2017 Apple Inc.  All rights reserved.
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

#define DEPRECATED_EXPORT_SYMBOLS 1
#include "WebKitDLL.h"

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#include "ForEachCoClass.h"
#include "resource.h"
#include "WebKit.h"
#include "WebKitClassFactory.h"
#include "WebStorageNamespaceProvider.h"
#include <WebCore/COMPtr.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/WebCoreInstanceHandle.h>
#include <WebCore/Widget.h>
#include <olectl.h>
#include <wchar.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

#if !PLATFORM(WIN_CAIRO)
#include <WebCore/RenderThemeWin.h>
#endif

using namespace WebCore;

ULONG gLockCount;
ULONG gClassCount;
HINSTANCE gInstance;

#define CLSID_FOR_CLASS(cls) CLSID_##cls,
CLSID gRegCLSIDs[] = {
    FOR_EACH_COCLASS(CLSID_FOR_CLASS)
};
#undef CLSID_FOR_CLASS

#if defined(DEPRECATED_EXPORT_SYMBOLS)

// Force symbols to be included so we can export them for legacy clients.
// DEPRECATED! People should get these symbols from JavaScriptCore.dll, not WebKit.dll!
typedef struct OpaqueJSClass* JSClassRef;
typedef const struct OpaqueJSContext* JSContextRef;
typedef const struct OpaqueJSValue* JSValueRef;
typedef struct OpaqueJSString* JSStringRef;
typedef wchar_t JSChar;
typedef unsigned JSPropertyAttributes;
struct JSClassDefinition;

#endif

HashCountedSet<String>& gClassNameCount()
{
    static NeverDestroyed<HashCountedSet<String>> gClassNameCount;
    return gClassNameCount.get();
}

void bindJavaScriptTrampoline();

STDAPI_(BOOL) DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID /*lpReserved*/)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            gLockCount = gClassCount = 0;
            gInstance = hModule;
            WebCore::setInstanceHandle(hModule);
            bindJavaScriptTrampoline();
            return TRUE;

        case DLL_PROCESS_DETACH:
#if !PLATFORM(WIN_CAIRO)
            WebCore::RenderThemeWin::setWebKitIsBeingUnloaded();
#endif
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return FALSE;
}

_Check_return_
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID* ppv)
{
    bool found = false;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(gRegCLSIDs); ++i) {
        if (IsEqualGUID(rclsid, gRegCLSIDs[i])) {
            found = true;
            break;
        }
    }
    if (!found)
        return E_FAIL;

    if (!IsEqualGUID(riid, IID_IUnknown) && !IsEqualGUID(riid, IID_IClassFactory))
        return E_NOINTERFACE;

    WebKitClassFactory* factory = new WebKitClassFactory(rclsid);
    *ppv = reinterpret_cast<LPVOID>(factory);
    if (!factory)
        return E_OUTOFMEMORY;

    factory->AddRef();
    return S_OK;
}

STDAPI DllCanUnloadNow(void)
{
    if (!gClassCount && !gLockCount)
        return S_OK;
    
    return S_FALSE;
}

// deprecated - do not use
STDAPI DllUnregisterServer(void)
{
    return 0;
}

// deprecated - do not use
STDAPI DllRegisterServer(void)
{
    return 0;
}

// deprecated - do not use
STDAPI RunAsLocalServer()
{
    return 0;
}

// deprecated - do not use
STDAPI LocalServerDidDie()
{
    return 0;
}

void shutDownWebKit()
{
    WebKit::WebStorageNamespaceProvider::closeLocalStorage();
}

#if !PLATFORM(WIN_CAIRO)
//FIXME: We should consider moving this to a new file for cross-project functionality
WEBKIT_API RefPtr<WebCore::FragmentedSharedBuffer> loadResourceIntoBuffer(const char* name);
RefPtr<WebCore::FragmentedSharedBuffer> loadResourceIntoBuffer(const char* name)
{
    int idr;
    // temporary hack to get resource id
    if (!strcmp(name, "textAreaResizeCorner"))
        idr = IDR_RESIZE_CORNER;
    else if (!strcmp(name, "missingImage"))
        idr = IDR_MISSING_IMAGE;
    else if (!strcmp(name, "nullPlugin"))
        idr = IDR_NULL_PLUGIN;
    else if (!strcmp(name, "panIcon"))
        idr = IDR_PAN_SCROLL_ICON;
    else if (!strcmp(name, "panSouthCursor"))
        idr = IDR_PAN_SOUTH_CURSOR;
    else if (!strcmp(name, "panNorthCursor"))
        idr = IDR_PAN_NORTH_CURSOR;
    else if (!strcmp(name, "panEastCursor"))
        idr = IDR_PAN_EAST_CURSOR;
    else if (!strcmp(name, "panWestCursor"))
        idr = IDR_PAN_WEST_CURSOR;
    else if (!strcmp(name, "panSouthEastCursor"))
        idr = IDR_PAN_SOUTH_EAST_CURSOR;
    else if (!strcmp(name, "panSouthWestCursor"))
        idr = IDR_PAN_SOUTH_WEST_CURSOR;
    else if (!strcmp(name, "panNorthEastCursor"))
        idr = IDR_PAN_NORTH_EAST_CURSOR;
    else if (!strcmp(name, "panNorthWestCursor"))
        idr = IDR_PAN_NORTH_WEST_CURSOR;
    else if (!strcmp(name, "searchMagnifier"))
        idr = IDR_SEARCH_MAGNIFIER;
    else if (!strcmp(name, "searchMagnifierResults"))
        idr = IDR_SEARCH_MAGNIFIER_RESULTS;
    else if (!strcmp(name, "searchCancel"))
        idr = IDR_SEARCH_CANCEL;
    else if (!strcmp(name, "searchCancelPressed"))
        idr = IDR_SEARCH_CANCEL_PRESSED;
    else if (!strcmp(name, "zoomInCursor"))
        idr = IDR_ZOOM_IN_CURSOR;
    else if (!strcmp(name, "zoomOutCursor"))
        idr = IDR_ZOOM_OUT_CURSOR;
    else if (!strcmp(name, "verticalTextCursor"))
        idr = IDR_VERTICAL_TEXT_CURSOR;
    else if (!strcmp(name, "fsVideoAudioVolumeHigh"))
        idr = IDR_FS_VIDEO_AUDIO_VOLUME_HIGH;
    else if (!strcmp(name, "fsVideoAudioVolumeLow"))
        idr = IDR_FS_VIDEO_AUDIO_VOLUME_LOW;
    else if (!strcmp(name, "fsVideoExitFullscreen"))
        idr = IDR_FS_VIDEO_EXIT_FULLSCREEN;
    else if (!strcmp(name, "fsVideoPause"))
        idr = IDR_FS_VIDEO_PAUSE;
    else if (!strcmp(name, "fsVideoPlay"))
        idr = IDR_FS_VIDEO_PLAY;
    else
        return nullptr;

    HRSRC resInfo = FindResource(gInstance, MAKEINTRESOURCE(idr), L"PNG");
    if (!resInfo)
        return nullptr;
    HANDLE res = LoadResource(gInstance, resInfo);
    if (!res)
        return nullptr;
    void* resource = LockResource(res);
    if (!resource)
        return nullptr;
    unsigned size = SizeofResource(gInstance, resInfo);

    return WebCore::SharedBuffer::create(reinterpret_cast<const char*>(resource), size);
}
#endif // !PLATFORM(WIN_CAIRO)

// Force symbols to be included so we can export them for legacy clients.
// DEPRECATED! People should get these symbols from JavaScriptCore.dll, not WebKit.dll!
// #include <JavaScriptCore/JSObjectRef.h>

typedef JSClassRef (*JSClassCreateFunction)(const JSClassDefinition* definition);
typedef void* (*JSObjectGetPrivateFunction)(JSObjectRef);
typedef JSObjectRef (*JSObjectMakeFunctionStub)(JSContextRef, JSClassRef, void*);
typedef void (*JSObjectSetPropertyFunction)(JSContextRef, JSObjectRef, JSStringRef propertyName, JSValueRef, JSPropertyAttributes, JSValueRef* exception);
#if USE(CF)
typedef JSStringRef (*JSStringCreateWithCFStringFunction)(CFStringRef);
#endif
typedef JSStringRef (*JSStringCreateWithUTF8CStringFunction)(const char*);
typedef const JSChar* (*JSStringGetCharactersPtrFunction)(JSStringRef);
typedef size_t (*JSStringGetLengthFunction)(JSStringRef);
typedef void (*JSStringReleaseFunction)(JSStringRef);
typedef bool (*JSValueIsTypeFunction)(JSContextRef, JSValueRef);
typedef JSValueRef(*JSValueMakeUndefinedFunction)(JSContextRef);
typedef double(*JSValueToNumberFunction)(JSContextRef, JSValueRef, JSValueRef* exception);
typedef JSValueRef(*JSValueMakeStringFunction)(JSContextRef, JSStringRef);
typedef JSStringRef (*JSValueToStringCopyFunction)(JSContextRef, JSValueRef, JSValueRef* exception);

JSClassCreateFunction m_jsClassCreateFunction = nullptr;
JSObjectGetPrivateFunction m_jsObjectGetPrivateFunction = nullptr;
JSObjectMakeFunctionStub m_jsObjectMakeFunction = nullptr;
JSObjectSetPropertyFunction m_jsObjectSetPropertyFunction = nullptr;
#if USE(CF)
JSStringCreateWithCFStringFunction m_jsStringCreateWithCFStringFunction = nullptr;
#endif
JSStringCreateWithUTF8CStringFunction m_jsStringCreateWithUTF8CStringFunction = nullptr;
JSStringGetCharactersPtrFunction m_jsStringGetCharactersPtrFunction = nullptr;
JSStringGetLengthFunction m_jsStringGetLengthFunction = nullptr;
JSStringReleaseFunction m_jsStringReleaseFunction = nullptr;
JSValueIsTypeFunction m_jsValueIsNumberFunction = nullptr;
JSValueIsTypeFunction m_jsValueIsStringFunction = nullptr;
JSValueMakeUndefinedFunction m_jsValueMakeUndefinedFunction = nullptr;
JSValueToNumberFunction m_jsValueToNumberFunction = nullptr;
JSValueMakeStringFunction m_jsValueMakeStringFunction = nullptr;
JSValueToStringCopyFunction m_jsValueToStringCopyFunction = nullptr;

void bindJavaScriptTrampoline()
{
    const wchar_t* libraryName = L"JavaScriptCore.dll";

    HMODULE library = ::LoadLibrary(libraryName);
    if (!library)
        return;

    m_jsClassCreateFunction = reinterpret_cast<JSClassCreateFunction>(::GetProcAddress(library, "JSClassCreate"));
    m_jsObjectGetPrivateFunction = reinterpret_cast<JSObjectGetPrivateFunction>(::GetProcAddress(library, "JSObjectGetPrivate"));
    m_jsObjectMakeFunction = reinterpret_cast<JSObjectMakeFunctionStub>(::GetProcAddress(library, "JSObjectMake"));
    m_jsObjectSetPropertyFunction = reinterpret_cast<JSObjectSetPropertyFunction>(::GetProcAddress(library, "JSObjectSetProperty"));;
#if USE(CF)
    m_jsStringCreateWithCFStringFunction = reinterpret_cast<JSStringCreateWithCFStringFunction>(::GetProcAddress(library, "JSStringCreateWithCFString"));
#endif
    m_jsStringCreateWithUTF8CStringFunction = reinterpret_cast<JSStringCreateWithUTF8CStringFunction>(::GetProcAddress(library, "JSStringCreateWithUTF8CString"));
    m_jsStringGetCharactersPtrFunction = reinterpret_cast<JSStringGetCharactersPtrFunction>(::GetProcAddress(library, "JSStringGetCharactersPtr"));
    m_jsStringGetLengthFunction = reinterpret_cast<JSStringGetLengthFunction>(::GetProcAddress(library, "JSStringGetLength"));
    m_jsStringReleaseFunction = reinterpret_cast<JSStringReleaseFunction>(::GetProcAddress(library, "JSStringRelease"));
    m_jsValueIsNumberFunction = reinterpret_cast<JSValueIsTypeFunction>(::GetProcAddress(library, "JSValueIsNumber"));
    m_jsValueIsStringFunction = reinterpret_cast<JSValueIsTypeFunction>(::GetProcAddress(library, "JSValueIsString"));
    m_jsValueMakeStringFunction = reinterpret_cast<JSValueMakeStringFunction>(::GetProcAddress(library, "JSValueMakeString"));
    m_jsValueMakeUndefinedFunction = reinterpret_cast<JSValueMakeUndefinedFunction>(::GetProcAddress(library, "JSValueMakeUndefined"));
    m_jsValueToNumberFunction = reinterpret_cast<JSValueToNumberFunction>(::GetProcAddress(library, "JSValueToNumber"));
    m_jsValueToStringCopyFunction = reinterpret_cast<JSValueToStringCopyFunction>(::GetProcAddress(library, "JSValueToStringCopy"));
}

extern "C"
{

WEBKIT_API JSClassRef JSClassCreate(const JSClassDefinition* definition)
{
    if (m_jsClassCreateFunction)
        return m_jsClassCreateFunction(definition);
    return nullptr;
}

WEBKIT_API void* JSObjectGetPrivate(JSObjectRef object)
{
    if (m_jsObjectGetPrivateFunction)
        return m_jsObjectGetPrivateFunction(object);
    return nullptr;
}

WEBKIT_API JSObjectRef JSObjectMake(JSContextRef ctx, JSClassRef classRef, void* data)
{
    if (m_jsObjectMakeFunction)
        return m_jsObjectMakeFunction(ctx, classRef, data);
    return nullptr;
}

WEBKIT_API void JSObjectSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception)
{
    if (m_jsObjectSetPropertyFunction)
        m_jsObjectSetPropertyFunction(ctx, object, propertyName, value, attributes, exception);
}

#if USE(CF)
WEBKIT_API JSStringRef JSStringCreateWithCFString(CFStringRef value)
{
    if (m_jsStringCreateWithCFStringFunction)
        return m_jsStringCreateWithCFStringFunction(value);
    return nullptr;
}
#endif

WEBKIT_API JSStringRef JSStringCreateWithUTF8CString(const char* value)
{
    if (m_jsStringCreateWithUTF8CStringFunction)
        return m_jsStringCreateWithUTF8CStringFunction(value);
    return nullptr;
}

WEBKIT_API const JSChar* JSStringGetCharactersPtr(JSStringRef value)
{
    if (m_jsStringGetCharactersPtrFunction)
        return m_jsStringGetCharactersPtrFunction(value);
    return nullptr;
}

WEBKIT_API size_t JSStringGetLength(JSStringRef value)
{
    if (m_jsStringGetLengthFunction)
        return m_jsStringGetLengthFunction(value);
    return 0;
}

WEBKIT_API void JSStringRelease(JSStringRef value)
{
    if (m_jsStringReleaseFunction)
        return m_jsStringReleaseFunction(value);
}

WEBKIT_API bool JSValueIsNumber(JSContextRef ctx, JSValueRef value)
{
    if (m_jsValueIsNumberFunction)
        return m_jsValueIsNumberFunction(ctx, value);
    return false;
}

WEBKIT_API bool JSValueIsString(JSContextRef ctx, JSValueRef value)
{
    if (m_jsValueIsStringFunction)
        return m_jsValueIsStringFunction(ctx, value);
    return false;
}

WEBKIT_API JSValueRef JSValueMakeString(JSContextRef ctx, JSStringRef value)
{
    if (m_jsValueMakeStringFunction)
        return m_jsValueMakeStringFunction(ctx, value);
    return nullptr;
}

WEBKIT_API JSValueRef JSValueMakeUndefined(JSContextRef ctx)
{
    if (m_jsValueMakeUndefinedFunction)
        return m_jsValueMakeUndefinedFunction(ctx);
    return nullptr;
}

WEBKIT_API double JSValueToNumber(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    if (m_jsValueToNumberFunction)
        return m_jsValueToNumberFunction(ctx, value, exception);
    return 0;
}

WEBKIT_API JSStringRef JSValueToStringCopy(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
    if (m_jsValueToStringCopyFunction)
        return m_jsValueToStringCopyFunction(ctx, value, exception);
    return nullptr;
}

}
