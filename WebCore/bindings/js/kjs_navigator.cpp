/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "kjs_navigator.h"

#include "AtomicString.h"
#include "CookieJar.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Language.h"
#include "Page.h"
#include "PluginInfoStore.h"
#include "Settings.h"
#include "kjs_window.h"
#include <kjs/object_object.h>

#ifndef WEBCORE_NAVIGATOR_PLATFORM
#if PLATFORM(MAC) && PLATFORM(PPC)
#define WEBCORE_NAVIGATOR_PLATFORM "MacPPC"
#elif PLATFORM(MAC) && PLATFORM(X86)
#define WEBCORE_NAVIGATOR_PLATFORM "MacIntel"
#elif PLATFORM(WIN_OS)
#define WEBCORE_NAVIGATOR_PLATFORM "Win32"
#else
#define WEBCORE_NAVIGATOR_PLATFORM ""
#endif
#endif // ifndef WEBCORE_NAVIGATOR_PLATFORM

#ifndef WEBCORE_NAVIGATOR_PRODUCT
#define WEBCORE_NAVIGATOR_PRODUCT "Gecko"
#endif // ifndef WEBCORE_NAVIGATOR_PRODUCT

#ifndef WEBCORE_NAVIGATOR_PRODUCT_SUB
#define WEBCORE_NAVIGATOR_PRODUCT_SUB "20030107"
#endif // ifndef WEBCORE_NAVIGATOR_PRODUCT_SUB

#ifndef WEBCORE_NAVIGATOR_VENDOR
#define WEBCORE_NAVIGATOR_VENDOR "Apple Computer, Inc."
#endif // ifndef WEBCORE_NAVIGATOR_VENDOR

#ifndef WEBCORE_NAVIGATOR_VENDOR_SUB
#define WEBCORE_NAVIGATOR_VENDOR_SUB ""
#endif // ifndef WEBCORE_NAVIGATOR_VENDOR_SUB

using namespace KJS;
using namespace WebCore;

namespace WebCore {

    class PluginBase : public DOMObject {
    public:
        PluginBase(ExecState*);
        virtual ~PluginBase();
        
        static void refresh(bool reload);

    protected:
        static void cachePluginDataIfNecessary();
        static Vector<PluginInfo*>* plugins;
        static Vector<MimeClassInfo*>* mimes;

    private:
        static int m_plugInCacheRefCount;
    };


    class Plugins : public PluginBase {
    public:
        Plugins(ExecState* exec) : PluginBase(exec) { }
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Length };
    private:
        static JSValue* indexGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
        static JSValue* nameGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    };

    JSValue* pluginsFunctionRefresh(ExecState*, JSObject*, const List&);

    class MimeTypes : public PluginBase {
    public:
        MimeTypes(ExecState* exec) : PluginBase(exec) { };
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Length };
    private:
        static JSValue* indexGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
        static JSValue* nameGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    };

    class Plugin : public PluginBase {
    public:
        Plugin(ExecState* exec, PluginInfo* info) : PluginBase(exec), m_info(info) { }
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Name, Filename, Description, Length };
    private:
        static JSValue* indexGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
        static JSValue* nameGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);

        PluginInfo* m_info;
    };

    class MimeType : public PluginBase {
    public:
        MimeType(ExecState* exec, MimeClassInfo* info) : PluginBase(exec), m_info(info) { }
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Type, Suffixes, Description, EnabledPlugin };
    private:
        MimeClassInfo* m_info;
    };

}

#include "kjs_navigator.lut.h"

namespace WebCore {

const ClassInfo Plugins::info = { "PluginArray", 0, &PluginsTable };
const ClassInfo MimeTypes::info = { "MimeTypeArray", 0, &MimeTypesTable };
const ClassInfo Plugin::info = { "Plugin", 0, &PluginTable };
const ClassInfo MimeType::info = { "MimeType", 0, &MimeTypeTable };

Vector<PluginInfo*>* PluginBase::plugins = 0;
Vector<MimeClassInfo*>* PluginBase::mimes = 0;
int PluginBase::m_plugInCacheRefCount = 0;

const ClassInfo Navigator::info = { "Navigator", 0, &NavigatorTable };
/*
@begin NavigatorTable 13
  appCodeName   Navigator::AppCodeName                  DontDelete|ReadOnly
  appName       Navigator::AppName                      DontDelete|ReadOnly
  appVersion    Navigator::AppVersion                   DontDelete|ReadOnly
  language      Navigator::Language                     DontDelete|ReadOnly
  userAgent     Navigator::UserAgent                    DontDelete|ReadOnly
  platform      Navigator::Platform                     DontDelete|ReadOnly
  plugins       Navigator::_Plugins                     DontDelete|ReadOnly
  mimeTypes     Navigator::_MimeTypes                   DontDelete|ReadOnly
  product       Navigator::Product                      DontDelete|ReadOnly
  productSub    Navigator::ProductSub                   DontDelete|ReadOnly
  vendor        Navigator::Vendor                       DontDelete|ReadOnly
  vendorSub     Navigator::VendorSub                    DontDelete|ReadOnly
  cookieEnabled Navigator::CookieEnabled                DontDelete|ReadOnly
  javaEnabled   navigatorProtoFuncJavaEnabled  DontDelete|Function 0
@end
*/

Navigator::Navigator(JSObject* prototype, Frame* frame)
    : DOMObject(prototype)
    , m_frame(frame)
{
}

bool Navigator::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticPropertySlot<Navigator, JSObject>(exec, &NavigatorTable, this, propertyName, slot);
}

static bool needsYouTubeQuirk(ExecState*, Frame*);

#if !PLATFORM(WIN)

static inline bool needsYouTubeQuirk(ExecState*, Frame*)
{
    return false;
}

#else

static bool needsYouTubeQuirk(ExecState* exec, Frame* frame)
{
    // This quirk works around a mistaken check in an ad at youtube.com.
    // There's a function called isSafari that returns false if the function
    // called isWindows returns true; thus the site malfunctions with Windows Safari.

    // Do the quirk only if the function's name is "isWindows".
    FunctionImp* function = exec->function();
    if (!function)
        return false;
    static const Identifier& isWindowsFunctionName = *new Identifier("isWindows");
    if (function->functionName() != isWindowsFunctionName)
        return false;

    // Do the quirk only if the function is called by an "isSafari" function.
    // However, that function is not itself named -- it is stored in the isSafari
    // property, though, so that's how we recognize it.
    ExecState* callingExec = exec->callingExecState();
    if (!callingExec)
        return false;
    FunctionImp* callingFunction = callingExec->function();
    if (!callingFunction)
        return false;
    JSObject* thisObject = callingExec->thisValue();
    if (!thisObject)
        return false;
    static const Identifier& isSafariFunctionName = *new Identifier("isSafari");
    JSValue* isSafariFunction = thisObject->getDirect(isSafariFunctionName);
    if (isSafariFunction != callingFunction)
        return false;

    Document* document = frame->document(); 
    // FIXME: The document is never null, so we should remove this check along with the
    // other similar ones in this file when we are absolutely sure it's safe.
    if (!document)
        return false;

    // Do the quirk only on the front page of the global version of YouTube.
    const KURL& url = document->url();
    if (url.host() != "youtube.com" && url.host() != "www.youtube.com")
        return false;
    if (url.path() != "/")
        return false;

    // As with other site-specific quirks, allow website developers to turn this off.
    // In theory, this allows website developers to check if their fixes are effective.
    Settings* settings = frame->settings(); 
    if (!settings)
        return false;
    if (!settings->needsSiteSpecificQuirks())
        return false;

    return true;
}

#endif

JSValue* Navigator::getValueProperty(ExecState* exec, int token) const
{
  switch (token) {
  case AppCodeName:
    return jsString("Mozilla");
  case AppName:
    return jsString("Netscape");
  case AppVersion: {
    if (needsYouTubeQuirk(exec, m_frame))
        return jsString("");
    // Version is everything in the user agent string past the "Mozilla/" prefix.
    const String userAgent = m_frame->loader()->userAgent(m_frame->document() ? m_frame->document()->url() : KURL());
    return jsString(userAgent.substring(userAgent.find('/') + 1));
  }
  case Product:
    return jsString(WEBCORE_NAVIGATOR_PRODUCT);
  case ProductSub:
    return jsString(WEBCORE_NAVIGATOR_PRODUCT_SUB);
  case Vendor:
    return jsString(WEBCORE_NAVIGATOR_VENDOR);
  case VendorSub:
    return jsString(WEBCORE_NAVIGATOR_VENDOR_SUB);
  case Language:
    return jsString(defaultLanguage());
  case UserAgent:
    return jsString(m_frame->loader()->userAgent(m_frame->document() ? m_frame->document()->url() : KURL()));
  case Platform:
    return jsString(WEBCORE_NAVIGATOR_PLATFORM);
  case _Plugins:
    return new Plugins(exec);
  case _MimeTypes:
    return new MimeTypes(exec);
  case CookieEnabled:
    return jsBoolean(cookiesEnabled(m_frame->document()));
  }
  return 0;
}

/*******************************************************************/

void PluginBase::cachePluginDataIfNecessary()
{
    if (!plugins) {
        plugins = new Vector<PluginInfo*>;
        mimes = new Vector<MimeClassInfo*>;
        
        // read configuration
        PluginInfoStore c;
        unsigned pluginCount = c.pluginCount();
        for (unsigned n = 0; n < pluginCount; n++) {
            PluginInfo* plugin = c.createPluginInfoForPluginAtIndex(n);
            if (!plugin) 
                continue;
            
            plugins->append(plugin);
            if (plugin->mimes.isEmpty())
                continue;
            
            Vector<MimeClassInfo*>::iterator end = plugin->mimes.end();
            for (Vector<MimeClassInfo*>::iterator itr = plugin->mimes.begin(); itr != end; itr++)
                mimes->append(*itr);
        }
    }
}

PluginBase::PluginBase(ExecState* exec)
    : DOMObject(exec->lexicalGlobalObject()->objectPrototype())
{
    cachePluginDataIfNecessary();
    m_plugInCacheRefCount++;
}

PluginBase::~PluginBase()
{
    m_plugInCacheRefCount--;
    if (!m_plugInCacheRefCount) {
        if (plugins) {
            deleteAllValues(*plugins);
            delete plugins;
            plugins = 0;
        }
        if (mimes) {
            deleteAllValues(*mimes);
            delete mimes;
            mimes = 0;
        }
    }
}

void PluginBase::refresh(bool reload)
{
    if (plugins) {
        deleteAllValues(*plugins);
        delete plugins;
        plugins = 0;
    }
    if (mimes) {
        deleteAllValues(*mimes);
        delete mimes;
        mimes = 0;
    }
    
    refreshPlugins(reload);
    cachePluginDataIfNecessary();
}


/*******************************************************************/

/*
@begin PluginsTable 2
  length        Plugins::Length                        DontDelete|ReadOnly
  refresh       pluginsFunctionRefresh                 DontDelete|Function 0
@end
*/

JSValue* Plugins::getValueProperty(ExecState* exec, int token) const
{
  ASSERT(token == Length);
  return jsNumber(plugins->size());
}

JSValue* Plugins::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    return new Plugin(exec, plugins->at(slot.index()));
}

JSValue* Plugins::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    AtomicString atomicPropertyName = propertyName;
    Vector<PluginInfo*>::iterator end = plugins->end();
    for (Vector<PluginInfo*>::iterator itr = plugins->begin(); itr != end; itr++) {
        PluginInfo* pl = *itr;
        if (pl->name == atomicPropertyName)
            return new Plugin(exec, pl);
    }
    return jsUndefined();
}

bool Plugins::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    const HashEntry* entry = Lookup::findEntry(&PluginsTable, propertyName);
    if (entry) {
      if (entry->attr & Function)
        slot.setStaticEntry(this, entry, staticFunctionGetter);
      else
        slot.setStaticEntry(this, entry, staticValueGetter<Plugins>);
      return true;
    } else {
        // plugins[#]
        bool ok;
        unsigned int i = propertyName.toUInt32(&ok);
        if (ok && i < plugins->size()) {
            slot.setCustomIndex(this, i, indexGetter);
            return true;
        }

        // plugin[name]
        AtomicString atomicPropertyName = propertyName;
        Vector<PluginInfo*>::iterator end = plugins->end();
        for (Vector<PluginInfo*>::iterator itr = plugins->begin(); itr != end; itr++) {
            if ((*itr)->name == atomicPropertyName) {
                slot.setCustom(this, nameGetter);
                return true;
            }
        }
    }

    return PluginBase::getOwnPropertySlot(exec, propertyName, slot);
}

/*******************************************************************/

/*
@begin MimeTypesTable 1
  length        MimeTypes::Length       DontDelete|ReadOnly
@end
*/

JSValue* MimeTypes::getValueProperty(ExecState* exec, int token) const
{
  ASSERT(token == Length);
  return jsNumber(mimes->size());
}

JSValue* MimeTypes::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    return new MimeType(exec, mimes->at(slot.index()));
}

JSValue* MimeTypes::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    AtomicString atomicPropertyName = propertyName;
    Vector<MimeClassInfo*>::iterator end = mimes->end();
    for (Vector<MimeClassInfo*>::iterator itr = mimes->begin(); itr != end; itr++) {
        MimeClassInfo* m = (*itr);
        if (m->type == atomicPropertyName)
            return new MimeType(exec, m);
    }
    return jsUndefined();
}

bool MimeTypes::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    const HashEntry* entry = Lookup::findEntry(&MimeTypesTable, propertyName);
    if (entry) {
      slot.setStaticEntry(this, entry, staticValueGetter<MimeTypes>);
      return true;
    } else {
        // mimeTypes[#]
        bool ok;
        unsigned int i = propertyName.toUInt32(&ok);
        if (ok && i < mimes->size()) {
            slot.setCustomIndex(this, i, indexGetter);
            return true;
        }

        // mimeTypes[name]
        AtomicString atomicPropertyName = propertyName;
        Vector<MimeClassInfo*>::iterator end = mimes->end();
        for (Vector<MimeClassInfo*>::iterator itr = mimes->begin(); itr != end; itr++) {
            if ((*itr)->type == atomicPropertyName) {
                slot.setCustom(this, nameGetter);
                return true;
            }
        }
    }

    return PluginBase::getOwnPropertySlot(exec, propertyName, slot);
}


/************************************************************************/

/*
@begin PluginTable 4
  name          Plugin::Name            DontDelete|ReadOnly
  filename      Plugin::Filename        DontDelete|ReadOnly
  description   Plugin::Description     DontDelete|ReadOnly
  length        Plugin::Length          DontDelete|ReadOnly
@end
*/

JSValue* Plugin::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
    case Name:
        return jsString(m_info->name);
    case Filename:
        return jsString(m_info->file);
    case Description:
        return jsString(m_info->desc);
    case Length: 
        return jsNumber(m_info->mimes.size());
    default:
        ASSERT(0);
        return jsUndefined();
    }
}

JSValue* Plugin::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    Plugin* thisObj = static_cast<Plugin*>(slot.slotBase());
    return new MimeType(exec, thisObj->m_info->mimes.at(slot.index()));
}

JSValue* Plugin::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    Plugin* thisObj = static_cast<Plugin*>(slot.slotBase());
    AtomicString atomicPropertyName = propertyName;
    Vector<MimeClassInfo*>::iterator end = thisObj->m_info->mimes.end();
    for (Vector<MimeClassInfo*>::iterator itr = thisObj->m_info->mimes.begin(); itr != end; itr++) {
        MimeClassInfo* m = (*itr);
        if (m->type == atomicPropertyName)
            return new MimeType(exec, m);
    }
    return jsUndefined();
}


bool Plugin::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    const HashEntry* entry = Lookup::findEntry(&PluginTable, propertyName);
    if (entry) {
        slot.setStaticEntry(this, entry, staticValueGetter<Plugin>);
        return true;
    } else {
        // plugin[#]
        bool ok;
        unsigned int i = propertyName.toUInt32(&ok);
        if (ok && i < m_info->mimes.size()) {
            slot.setCustomIndex(this, i, indexGetter);
            return true;
        }

        // plugin["name"]
        AtomicString atomicPropertyName = propertyName;
        Vector<MimeClassInfo*>::iterator end = m_info->mimes.end();
        for (Vector<MimeClassInfo*>::iterator itr = m_info->mimes.begin(); itr != end; itr++) {
            if ((*itr)->type == atomicPropertyName) {
                slot.setCustom(this, nameGetter);
                return true;
            }
        }
    }

    return PluginBase::getOwnPropertySlot(exec, propertyName, slot);
}

/*****************************************************************************/

/*
@begin MimeTypeTable 4
  type          MimeType::Type          DontDelete|ReadOnly
  suffixes      MimeType::Suffixes      DontDelete|ReadOnly
  description   MimeType::Description   DontDelete|ReadOnly
  enabledPlugin MimeType::EnabledPlugin DontDelete|ReadOnly
@end
*/

JSValue* MimeType::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
    case Type:
        return jsString(m_info->type);
    case Suffixes:
        return jsString(m_info->suffixes);
    case Description:
        return jsString(m_info->desc);
    case EnabledPlugin: {
        Frame* frame = Window::retrieveActive(exec)->impl()->frame();
        ASSERT(frame);
        Settings* settings = frame->settings();
        if (settings && settings->arePluginsEnabled())
            return new Plugin(exec, m_info->plugin);
        else
            return jsUndefined();
    }
    default:
        return jsUndefined();
    }
}

bool MimeType::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<MimeType, PluginBase>(exec, &MimeTypeTable, this, propertyName, slot);
}

JSValue* pluginsFunctionRefresh(ExecState* exec, JSObject*, const List& args)
{
    PluginBase::refresh(args[0]->toBoolean(exec));
    return jsUndefined();
}

JSValue* navigatorProtoFuncJavaEnabled(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj->inherits(&Navigator::info))
        return throwError(exec, TypeError);
    Navigator* nav = static_cast<Navigator*>(thisObj);
    Settings* settings = nav->frame() ? nav->frame()->settings() : 0;
    return jsBoolean(settings && settings->isJavaEnabled());
}

} // namespace
