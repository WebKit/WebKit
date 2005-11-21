// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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
#include <klocale.h>

#include <kstandarddirs.h>
#include <kconfig.h>
#include <kdebug.h>

#include <kio/kprotocolmanager.h>
#include "kjs_navigator.h"
#include "kjs/lookup.h"
#include "kjs_binding.h"
#include "khtml_part.h"

#include "KWQKCookieJar.h"

namespace KJS {

    class PluginBase : public ObjectImp {
    public:
        PluginBase(ExecState *exec);
        virtual ~PluginBase();
        
        void refresh(bool reload);

        struct MimeClassInfo;
        struct PluginInfo;

        struct MimeClassInfo {
            QString type;
            QString desc;
            QString suffixes;
            PluginInfo *plugin;
        };

        struct PluginInfo {
            QString name;
            QString file;
            QString desc;
            QPtrList<MimeClassInfo> mimes;
        };

        static QPtrList<PluginInfo> *plugins;
        static QPtrList<MimeClassInfo> *mimes;

    private:
        static int m_refCount;
    };


    class Plugins : public PluginBase {
    public:
        Plugins(ExecState *exec) : PluginBase(exec) {};
        virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
        ValueImp *getValueProperty(ExecState *, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Length, Refresh };
    private:
        static ValueImp *indexGetter(ExecState *, const Identifier&, const PropertySlot&);
        static ValueImp *nameGetter(ExecState *, const Identifier&, const PropertySlot&);
    };

    class MimeTypes : public PluginBase {
    public:
        MimeTypes(ExecState *exec) : PluginBase(exec) { };
        virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
        ValueImp *getValueProperty(ExecState *, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Length };
    private:
        static ValueImp *indexGetter(ExecState *, const Identifier&, const PropertySlot&);
        static ValueImp *nameGetter(ExecState *, const Identifier&, const PropertySlot&);
    };

    class Plugin : public PluginBase {
    public:
        Plugin(ExecState *exec, PluginInfo *info) : PluginBase(exec), m_info(info) { }
        virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
        ValueImp *getValueProperty(ExecState *, int token) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        enum { Name, Filename, Description, Length };
    private:
        static ValueImp *indexGetter(ExecState *, const Identifier&, const PropertySlot&);
        static ValueImp *nameGetter(ExecState *, const Identifier&, const PropertySlot&);

        PluginInfo *m_info;
    };

    class MimeType : public PluginBase {
    public:
        MimeType( ExecState *exec, MimeClassInfo *info ) : PluginBase(exec), m_info(info) { }
        virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
        ValueImp *getValueProperty(ExecState *, int token) const;
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

QPtrList<PluginBase::PluginInfo> *KJS::PluginBase::plugins = 0;
QPtrList<PluginBase::MimeClassInfo> *KJS::PluginBase::mimes = 0;
int KJS::PluginBase::m_refCount = 0;

const ClassInfo Navigator::info = { "Navigator", 0, &NavigatorTable, 0 };
/*
@begin NavigatorTable 13
  appCodeName	Navigator::AppCodeName	DontDelete|ReadOnly
  appName	Navigator::AppName	DontDelete|ReadOnly
  appVersion	Navigator::AppVersion	DontDelete|ReadOnly
  language	Navigator::Language	DontDelete|ReadOnly
  userAgent	Navigator::UserAgent	DontDelete|ReadOnly
  platform	Navigator::Platform	DontDelete|ReadOnly
  plugins	Navigator::_Plugins	DontDelete|ReadOnly
  mimeTypes	Navigator::_MimeTypes	DontDelete|ReadOnly
  product	Navigator::Product	DontDelete|ReadOnly
  productSub	Navigator::ProductSub	DontDelete|ReadOnly
  vendor	Navigator::Vendor	DontDelete|ReadOnly
  cookieEnabled	Navigator::CookieEnabled DontDelete|ReadOnly
  javaEnabled	Navigator::JavaEnabled	DontDelete|Function 0
@end
*/
IMPLEMENT_PROTOFUNC(NavigatorFunc)

Navigator::Navigator(ExecState *exec, KHTMLPart *p)
  : ObjectImp(exec->lexicalInterpreter()->builtinObjectPrototype()), m_part(p) { }

bool Navigator::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticPropertySlot<NavigatorFunc, Navigator, ObjectImp>(exec, &NavigatorTable, this, propertyName, slot);
}

ValueImp *Navigator::getValueProperty(ExecState *exec, int token) const
{
  QString userAgent = KWQ(m_part)->userAgent();
  switch (token) {
  case AppCodeName:
    return String("Mozilla");
  case AppName:
    // If we find "Mozilla" but not "(compatible, ...)" we are a real Netscape
    if (userAgent.find(QString::fromLatin1("Mozilla")) >= 0 &&
        userAgent.find(QString::fromLatin1("compatible")) == -1)
    {
      //kdDebug() << "appName -> Mozilla" << endl;
      return String("Netscape");
    }
    if (userAgent.find(QString::fromLatin1("Microsoft")) >= 0 ||
        userAgent.find(QString::fromLatin1("MSIE")) >= 0)
    {
      //kdDebug() << "appName -> IE" << endl;
      return String("Microsoft Internet Explorer");
    }
    // FIXME: Should we define a fallback result here besides "Konqueror"?
    return Undefined();
  case AppVersion:
    // We assume the string is something like Mozilla/version (properties)
    return String(userAgent.mid(userAgent.find('/') + 1));
  case Product:
    // When acting normal, we pretend to be "Gecko".
    if (userAgent.find("Mozilla/5.0") >= 0 && userAgent.find("compatible") == -1) {
        return String("Gecko");
    }
    // When spoofing as IE, we use Undefined().
    return Undefined();
  case ProductSub:
    return String("20030107");
  case Vendor:
    return String("Apple Computer, Inc.");
  case Language:
    // We don't have an implementation of KGlobal::locale().  We do however
    // have a static method on KLocale to access the current language.
    return String(KLocale::language());
  case UserAgent:
    return String(userAgent);
  case Platform:
    // yet another evil hack, but necessary to spoof some sites...
    if ( (userAgent.find(QString::fromLatin1("Win"),0,false)>=0) )
      return String(QString::fromLatin1("Win32"));
    else if ( (userAgent.find(QString::fromLatin1("Macintosh"),0,false)>=0) ||
              (userAgent.find(QString::fromLatin1("Mac_PowerPC"),0,false)>=0) )
      return String(QString::fromLatin1("MacPPC"));
    else
      return String(QString::fromLatin1("X11"));
  case _Plugins:
    return new Plugins(exec);
  case _MimeTypes:
    return new MimeTypes(exec);
  case CookieEnabled:
    return Boolean(KWQKCookieJar::cookieEnabled());
  default:
    kdWarning() << "Unhandled token in DOMEvent::getValueProperty : " << token << endl;
    return NULL;
  }
}

/*******************************************************************/

PluginBase::PluginBase(ExecState *exec)
  : ObjectImp(exec->lexicalInterpreter()->builtinObjectPrototype() )
{
    if ( !plugins ) {
        plugins = new QPtrList<PluginInfo>;
        mimes = new QPtrList<MimeClassInfo>;
        plugins->setAutoDelete( true );
        mimes->setAutoDelete( true );

        // read configuration
        KConfig c(KGlobal::dirs()->saveLocation("data","nsplugins")+"/pluginsinfo");
        unsigned num = (unsigned int)c.readNumEntry("number");
        for ( unsigned n=0; n<num; n++ ) {

            c.setGroup( QString::number(n) );
            PluginInfo *plugin = new PluginInfo;

            plugin->name = c.readEntry("name");
            plugin->file = c.readEntry("file");
            plugin->desc = c.readEntry("description");

            //kdDebug(6070) << "plugin : " << plugin->name << " - " << plugin->desc << endl;

            plugins->append( plugin );

            // get mime types from string
            QStringList types = QStringList::split( ';', c.readEntry("mime") );
            QStringList::Iterator type;
            for ( type=types.begin(); type!=types.end(); ++type ) {

                // get mime information
                MimeClassInfo *mime = new MimeClassInfo;
                QStringList tokens = QStringList::split(':', *type, TRUE);
                QStringList::Iterator token;

                token = tokens.begin();
                if (token == tokens.end()) {
                    delete mime;
                    continue;
                }
                mime->type = (*token).lower();
                //kdDebug(6070) << "mime->type=" << mime->type << endl;
                ++token;

                if (token == tokens.end()) {
                    delete mime;
                    continue;
                }
                mime->suffixes = *token;
                ++token;

                if (token == tokens.end()) {
                    delete mime;
                    continue;
                }
                mime->desc = *token;
                ++token;

                mime->plugin = plugin;

                mimes->append( mime );
                plugin->mimes.append( mime );
            }
        }
    }

    m_refCount++;
}

PluginBase::~PluginBase()
{
    m_refCount--;
    if ( m_refCount==0 ) {
        delete plugins;
        delete mimes;
        plugins = 0;
        mimes = 0;
    }
}

void PluginBase::refresh(bool reload)
{
    delete plugins;
    delete mimes;
    plugins = 0;
    mimes = 0;
    RefreshPlugins(reload);
}


/*******************************************************************/

/*
@begin PluginsTable 2
  length	Plugins::Length		DontDelete|ReadOnly
  refresh	Plugins::Refresh	DontDelete|Function 0
@end
*/
IMPLEMENT_PROTOFUNC(PluginsFunc)

ValueImp *Plugins::getValueProperty(ExecState *exec, int token) const
{
  assert(token == Length);
  return Number(plugins->count());
}

ValueImp *Plugins::indexGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    return new Plugin(exec, plugins->at(slot.index()));
}

ValueImp *Plugins::nameGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
  for (PluginInfo *pl = plugins->first(); pl; pl = plugins->next()) {
    if (pl->name == propertyName.qstring()) {
      return new Plugin(exec, pl);
    }
  }
  return Undefined();
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
        if (ok && i < plugins->count()) {
            slot.setCustomIndex(this, i, indexGetter);
            return true;
        }

        // plugin[name]
        for (PluginInfo *pl = plugins->first(); pl; pl = plugins->next()) {
            if (pl->name == propertyName.qstring()) {
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
  length	MimeTypes::Length	DontDelete|ReadOnly
@end
*/

ValueImp *MimeTypes::getValueProperty(ExecState *exec, int token) const
{
  assert(token == Length);
  return Number(plugins->count());
}

ValueImp *MimeTypes::indexGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    return new MimeType(exec, mimes->at(slot.index()));
}

ValueImp *MimeTypes::nameGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
  for (MimeClassInfo *m = mimes->first(); m; m = mimes->next()) {
      if (m->type == propertyName.qstring())
          return new MimeType(exec, m);
  }
  return Undefined();
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
        if (ok && i < mimes->count()) {
            slot.setCustomIndex(this, i, indexGetter);
            return true;
        }

        // mimeTypes[name]
        for (MimeClassInfo *m = mimes->first(); m; m = mimes->next()) {
            if (m->type == propertyName.qstring()) {
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
  name		Plugin::Name		DontDelete|ReadOnly
  filename	Plugin::Filename	DontDelete|ReadOnly
  description	Plugin::Description	DontDelete|ReadOnly
  length	Plugin::Length		DontDelete|ReadOnly
@end
*/

ValueImp *Plugin::getValueProperty(ExecState *exec, int token) const
{
    switch (token) {
    case Name:
        return String(m_info->name);
    case Filename:
        return String(m_info->file);
    case Description:
        return String(m_info->desc);
    case Length: 
        return Number(m_info->mimes.count());
    default:
        assert(0);
        return Undefined();
    }
}

ValueImp *Plugin::indexGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    Plugin *thisObj = static_cast<Plugin *>(slot.slotBase());
    return new MimeType(exec, thisObj->m_info->mimes.at(slot.index()));
}

ValueImp *Plugin::nameGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
    Plugin *thisObj = static_cast<Plugin *>(slot.slotBase());
    for (MimeClassInfo *m = thisObj->m_info->mimes.first(); m; m = thisObj->m_info->mimes.next()) {
        if (m->type == propertyName.qstring())
            return new MimeType(exec, m);
    }
    return Undefined();
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
        if (ok && i < m_info->mimes.count()) {
            slot.setCustomIndex(this, i, indexGetter);
            return true;
        }

        // plugin["name"]
        for (MimeClassInfo *m=m_info->mimes.first(); m; m = m_info->mimes.next()) {
            if (m->type == propertyName.qstring()) {
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
  type		MimeType::Type		DontDelete|ReadOnly
  suffixes	MimeType::Suffixes	DontDelete|ReadOnly
  description	MimeType::Description	DontDelete|ReadOnly
  enabledPlugin	MimeType::EnabledPlugin	DontDelete|ReadOnly
@end
*/

ValueImp *MimeType::getValueProperty(ExecState *exec, int token) const
{
    switch (token) {
    case Type:
        return String(m_info->type);
    case Suffixes:
        return String(m_info->suffixes);
    case Description:
        return String(m_info->desc);
    case EnabledPlugin:
        return new Plugin(exec, m_info->plugin);
    default:
        return Undefined();
    }
}

bool MimeType::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<MimeType, PluginBase>(exec, &MimeTypeTable, this, propertyName, slot);
}

ValueImp *PluginsFunc::callAsFunction(ExecState *exec, ObjectImp *, const List &args)
{
    PluginBase(exec).refresh(args[0]->toBoolean(exec));
    return Undefined();
}

ValueImp *NavigatorFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &)
{
  if (!thisObj->inherits(&KJS::Navigator::info))
    return throwError(exec, TypeError);
  Navigator *nav = static_cast<Navigator *>(thisObj);
  // javaEnabled()
  return Boolean(nav->part()->javaEnabled());
}

} // namespace
