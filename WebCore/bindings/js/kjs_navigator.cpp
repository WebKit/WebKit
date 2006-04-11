// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "kjs_navigator.h"

#include "AtomicString.h"
#include "CookieJar.h"
#include "Frame.h"
#include "Language.h"
#include "PlugInInfoStore.h"

using namespace WebCore;

namespace KJS {

    class PluginBase : public DOMObject {
    public:
        PluginBase(ExecState *exec);
        virtual ~PluginBase();
        
        void refresh(bool reload);

    protected:
        static void cachePluginDataIfNecessary();
        static Vector<PluginInfo*> *plugins;
        static Vector<MimeClassInfo*> *mimes;

    private:
        static int m_plugInCacheRefCount;
    };


    class Plugins : public PluginBase {
    public:
        Plugins(ExecState *exec) : PluginBase(exec) {};
        virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
        JSValue *getValueProperty(ExecState *, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Length, Refresh };
    private:
        static JSValue *indexGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
        static JSValue *nameGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    };

    class MimeTypes : public PluginBase {
    public:
        MimeTypes(ExecState *exec) : PluginBase(exec) { };
        virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
        JSValue *getValueProperty(ExecState *, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Length };
    private:
        static JSValue *indexGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
        static JSValue *nameGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    };

    class Plugin : public PluginBase {
    public:
        Plugin(ExecState *exec, PluginInfo *info) : PluginBase(exec), m_info(info) { }
        virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
        JSValue *getValueProperty(ExecState *, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Name, Filename, Description, Length };
    private:
        static JSValue *indexGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
        static JSValue *nameGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);

        PluginInfo *m_info;
    };

    class MimeType : public PluginBase {
    public:
        MimeType( ExecState *exec, MimeClassInfo *info ) : PluginBase(exec), m_info(info) { }
        virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
        JSValue *getValueProperty(ExecState *, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Type, Suffixes, Description, EnabledPlugin };
    private:
        MimeClassInfo *m_info;
    };

} // namespace

#include "kjs_navigator.lut.h"

namespace KJS {

const ClassInfo Plugins::info = { "PluginArray", 0, &PluginsTable, 0 };
const ClassInfo MimeTypes::info = { "MimeTypeArray", 0, &MimeTypesTable, 0 };
const ClassInfo Plugin::info = { "Plugin", 0, &PluginTable, 0 };
const ClassInfo MimeType::info = { "MimeType", 0, &MimeTypeTable, 0 };

Vector<PluginInfo*> *KJS::PluginBase::plugins = 0;
Vector<MimeClassInfo*> *KJS::PluginBase::mimes = 0;
int KJS::PluginBase::m_plugInCacheRefCount = 0;

const ClassInfo Navigator::info = { "Navigator", 0, &NavigatorTable, 0 };
/*
@begin NavigatorTable 13
  appCodeName   Navigator::AppCodeName  DontDelete|ReadOnly
  appName       Navigator::AppName      DontDelete|ReadOnly
  appVersion    Navigator::AppVersion   DontDelete|ReadOnly
  language      Navigator::Language     DontDelete|ReadOnly
  userAgent     Navigator::UserAgent    DontDelete|ReadOnly
  platform      Navigator::Platform     DontDelete|ReadOnly
  plugins       Navigator::_Plugins     DontDelete|ReadOnly
  mimeTypes     Navigator::_MimeTypes   DontDelete|ReadOnly
  product       Navigator::Product      DontDelete|ReadOnly
  productSub    Navigator::ProductSub   DontDelete|ReadOnly
  vendor        Navigator::Vendor       DontDelete|ReadOnly
  vendorSub     Navigator::VendorSub    DontDelete|ReadOnly
  cookieEnabled Navigator::CookieEnabled DontDelete|ReadOnly
  javaEnabled   Navigator::JavaEnabled  DontDelete|Function 0
@end
*/
KJS_IMPLEMENT_PROTOFUNC(NavigatorFunc)

Navigator::Navigator(ExecState *exec, Frame *f) 
    : m_frame(f)
{
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
}

bool Navigator::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticPropertySlot<NavigatorFunc, Navigator, JSObject>(exec, &NavigatorTable, this, propertyName, slot);
}

JSValue *Navigator::getValueProperty(ExecState *exec, int token) const
{
  String userAgent = m_frame->userAgent();
  switch (token) {
  case AppCodeName:
    return jsString("Mozilla");
  case AppName:
    // If we find "Mozilla" but not "(compatible, ...)" we are a real Netscape
    if (userAgent.find("Mozilla") >= 0 && userAgent.find("compatible") == -1)
      return jsString("Netscape");
    if (userAgent.find("Microsoft") >= 0 || userAgent.find("MSIE") >= 0)
      return jsString("Microsoft Internet Explorer");
    return jsUndefined();
  case AppVersion:
    // We assume the string is something like Mozilla/version (properties)
    return jsString(userAgent.substring(userAgent.find('/') + 1));
  case Product:
    // When acting normal, we pretend to be "Gecko".
    if (userAgent.find("Mozilla/5.0") >= 0 && userAgent.find("compatible") == -1)
        return jsString("Gecko");
    // When spoofing as IE, we use jsUndefined().
    return jsUndefined();
  case ProductSub:
    return jsString("20030107");
  case Vendor:
    return jsString("Apple Computer, Inc.");
  case VendorSub:
    return jsString("");
  case Language:
    return jsString(defaultLanguage());
  case UserAgent:
    return jsString(userAgent);
  case Platform:
    if (userAgent.find("Win", 0, false) >= 0)
      return jsString("Win32");
    if (userAgent.find("Macintosh", 0, false) >= 0 || userAgent.find("Mac_PowerPC", 0, false) >= 0)
      return jsString("MacPPC");
    // FIXME: What about Macintosh Intel?
    return jsString("X11");
  case _Plugins:
    return new Plugins(exec);
  case _MimeTypes:
    return new MimeTypes(exec);
  case CookieEnabled:
    return jsBoolean(cookiesEnabled());
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
        PlugInInfoStore c;
        unsigned pluginCount = c.pluginCount();
        for (unsigned n = 0; n < pluginCount; n++) {
            PluginInfo* plugin = c.createPluginInfoForPluginAtIndex(n);
            if (!plugin) 
                continue;
            
            plugins->append(plugin);
            if (!plugin->mimes)
                continue;
            
            Vector<MimeClassInfo*>::iterator end = plugin->mimes.end();
            for (Vector<MimeClassInfo*>::iterator itr = plugin->mimes.begin(); itr != end; itr++)
                mimes->append(*itr);
        }
    }
}

PluginBase::PluginBase(ExecState *exec)
{
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());

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
  length        Plugins::Length         DontDelete|ReadOnly
  refresh       Plugins::Refresh        DontDelete|Function 0
@end
*/
KJS_IMPLEMENT_PROTOFUNC(PluginsFunc)

JSValue *Plugins::getValueProperty(ExecState *exec, int token) const
{
  assert(token == Length);
  return jsNumber(plugins->size());
}

JSValue *Plugins::indexGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    return new Plugin(exec, plugins->at(slot.index()));
}

JSValue *Plugins::nameGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    AtomicString atomicPropertyName = propertyName;
    Vector<PluginInfo*>::iterator end = plugins->end();
    for (Vector<PluginInfo*>::iterator itr = plugins->begin(); itr != end; itr++) {
        PluginInfo *pl = *itr;
        if (pl->name == atomicPropertyName)
            return new Plugin(exec, pl);
    }
    return jsUndefined();
}

bool Plugins::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    const HashEntry* entry = Lookup::findEntry(&PluginsTable, propertyName);
    if (entry) {
      if (entry->attr & Function)
        slot.setStaticEntry(this, entry, staticFunctionGetter<PluginsFunc>);
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

JSValue *MimeTypes::getValueProperty(ExecState *exec, int token) const
{
  assert(token == Length);
  return jsNumber(plugins->size());
}

JSValue *MimeTypes::indexGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    return new MimeType(exec, mimes->at(slot.index()));
}

JSValue *MimeTypes::nameGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    AtomicString atomicPropertyName = propertyName;
    Vector<MimeClassInfo*>::iterator end = mimes->end();
    for (Vector<MimeClassInfo*>::iterator itr = mimes->begin(); itr != end; itr++) {
        MimeClassInfo *m = (*itr);
        if (m->type == atomicPropertyName)
            return new MimeType(exec, m);
    }
    return jsUndefined();
}

bool MimeTypes::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    const HashEntry* entry = Lookup::findEntry(&MimeTypesTable, propertyName);
    if (entry) {
      slot.setStaticEntry(this, entry, staticValueGetter<Plugins>);
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

JSValue *Plugin::getValueProperty(ExecState *exec, int token) const
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
        assert(0);
        return jsUndefined();
    }
}

JSValue *Plugin::indexGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    Plugin *thisObj = static_cast<Plugin *>(slot.slotBase());
    return new MimeType(exec, thisObj->m_info->mimes.at(slot.index()));
}

JSValue *Plugin::nameGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    Plugin *thisObj = static_cast<Plugin *>(slot.slotBase());
    AtomicString atomicPropertyName = propertyName;
    Vector<MimeClassInfo*>::iterator end = thisObj->m_info->mimes.end();
    for (Vector<MimeClassInfo*>::iterator itr = thisObj->m_info->mimes.begin(); itr != end; itr++) {
        MimeClassInfo *m = (*itr);
        if (m->type == atomicPropertyName)
            return new MimeType(exec, m);
    }
    return jsUndefined();
}


bool Plugin::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
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

JSValue *MimeType::getValueProperty(ExecState *exec, int token) const
{
    switch (token) {
    case Type:
        return jsString(m_info->type);
    case Suffixes:
        return jsString(m_info->suffixes);
    case Description:
        return jsString(m_info->desc);
    case EnabledPlugin: {
        ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());
        Frame *frame = interpreter->frame();
        if (frame && frame->pluginsEnabled())
            return new Plugin(exec, m_info->plugin);
        else
            return jsUndefined();
    }
    default:
        return jsUndefined();
    }
}

bool MimeType::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<MimeType, PluginBase>(exec, &MimeTypeTable, this, propertyName, slot);
}

JSValue *PluginsFunc::callAsFunction(ExecState *exec, JSObject *, const List &args)
{
    PluginBase(exec).refresh(args[0]->toBoolean(exec));
    return jsUndefined();
}

JSValue *NavigatorFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &)
{
  if (!thisObj->inherits(&KJS::Navigator::info))
    return throwError(exec, TypeError);
  Navigator *nav = static_cast<Navigator *>(thisObj);
  // javaEnabled()
  return jsBoolean(nav->frame()->javaEnabled());
}

} // namespace
