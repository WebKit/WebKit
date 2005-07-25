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

#include <klocale.h>

#include <kstandarddirs.h>
#include <kconfig.h>
#include <kdebug.h>

#include <kio/kprotocolmanager.h>
#include "kjs_navigator.h"
#include "kjs/lookup.h"
#include "kjs_navigator.lut.h"
#include "kjs_binding.h"
#include "khtml_part.h"

#if APPLE_CHANGES
#include "KWQKCookieJar.h"
#endif

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
        virtual bool getOwnProperty(ExecState *exec, const Identifier& propertyName, Value& result) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
    private:
    };
    const ClassInfo Plugins::info = { "PluginArray", 0, 0, 0 };


    class MimeTypes : public PluginBase {
    public:
        MimeTypes(ExecState *exec) : PluginBase(exec) { };
        virtual bool getOwnProperty(ExecState *exec, const Identifier& propertyName, Value& result) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
    private:
    };
    const ClassInfo MimeTypes::info = { "MimeTypeArray", 0, 0, 0 };


    class Plugin : public PluginBase {
    public:
        Plugin(ExecState *exec, PluginInfo *info) : PluginBase(exec), m_info(info) { }
        virtual bool getOwnProperty(ExecState *exec, const Identifier& propertyName, Value& result) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
    private:
        PluginInfo *m_info;
    };
    const ClassInfo Plugin::info = { "Plugin", 0, 0, 0 };


    class MimeType : public PluginBase {
    public:
        MimeType( ExecState *exec, MimeClassInfo *info ) : PluginBase(exec), m_info(info) { }
        virtual bool getOwnProperty(ExecState *exec, const Identifier& propertyName, Value& result) const;
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
    private:
        MimeClassInfo *m_info;
    };
    const ClassInfo MimeType::info = { "MimeType", 0, 0, 0 };

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

bool Navigator::getOwnProperty(ExecState *exec, const Identifier& propertyName, Value& result) const
{
  return lookupGetOwnProperty<NavigatorFunc, Navigator, ObjectImp>(exec, propertyName, &NavigatorTable, this, result);
}

Value Navigator::getValueProperty(ExecState *exec, int token) const
{
#if APPLE_CHANGES
  QString userAgent = KWQ(m_part)->userAgent();
#else
  KURL url = m_part->url();
  QString userAgent = KProtocolManager::userAgentForHost(url.host());
#endif
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
#if APPLE_CHANGES
    // FIXME: Should we define a fallback result here besides "Konqueror"?
    return Undefined();
#else
    //kdDebug() << "appName -> Konqueror" << endl;
    return String("Konqueror");
#endif
  case AppVersion:
    // We assume the string is something like Mozilla/version (properties)
    return String(userAgent.mid(userAgent.find('/') + 1));
  case Product:
#if APPLE_CHANGES
    // When acting normal, we pretend to be "Gecko".
    if (userAgent.find("Mozilla/5.0") >= 0 && userAgent.find("compatible") == -1) {
        return String("Gecko");
    }
    // When spoofing as IE, we use Undefined().
    return Undefined();
#else
    return String("Konqueror/khtml");
#endif
  case ProductSub:
    return String("20030107");
  case Vendor:
#if APPLE_CHANGES
    return String("Apple Computer, Inc.");
#else
    return String("KDE");
#endif
  case Language:
#if APPLE_CHANGES
    // We don't have an implementation of KGlobal::locale().  We do however
    // have a static method on KLocale to access the current language.
    return String(KLocale::language());
#else
    return String(KGlobal::locale()->language() == "C" ?
                  QString::fromLatin1("en") : KGlobal::locale()->language());
#endif
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
    return Value(new Plugins(exec));
  case _MimeTypes:
    return Value(new MimeTypes(exec));
  case CookieEnabled:
#if APPLE_CHANGES
    return Boolean(KWQKCookieJar::cookieEnabled());
#else
    return Boolean(true); /// ##### FIXME
#endif
  default:
    kdWarning() << "Unhandled token in DOMEvent::getValueProperty : " << token << endl;
    return Value();
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
#if APPLE_CHANGES
    RefreshPlugins(reload);
#endif
}


/*******************************************************************/
IMPLEMENT_PROTOFUNC(PluginsFunc)

bool Plugins::getOwnProperty(ExecState *exec, const Identifier& propertyName, Value& result) const
{
    if (propertyName == "refresh") {
        result = lookupOrCreateFunction<PluginsFunc>(exec, propertyName, this, 0, 0, DontDelete|Function);
        return true;
    } else if (propertyName == lengthPropertyName) {
        result = Number(plugins->count());
        return true;
    } else {
        // plugins[#]
        bool ok;
        unsigned int i = propertyName.toULong(&ok);
        if (ok && i < plugins->count()) {
            result = Value(new Plugin(exec, plugins->at(i)));
            return true;
        }

        // plugin[name]
        for (PluginInfo *pl = plugins->first(); pl; pl = plugins->next()) {
            if (pl->name==propertyName.qstring()) {
                result = Value(new Plugin(exec, pl));
                return true;
            }
        }
    }

    return PluginBase::getOwnProperty(exec, propertyName, result);
}

/*******************************************************************/

bool MimeTypes::getOwnProperty(ExecState *exec, const Identifier& propertyName, Value& result) const
{
    if (propertyName == lengthPropertyName) {
        result = Number(mimes->count());
        return true;
    } else {
        // mimeTypes[#]
        bool ok;
        unsigned int i = propertyName.toULong(&ok);
        if (ok && i < mimes->count()) {
            result = Value(new MimeType(exec, mimes->at(i)));
            return true;
        }

        // mimeTypes[name]
        for (MimeClassInfo *m = mimes->first(); m; m = mimes->next()) {
            if (m->type == propertyName.qstring()) {
                result = Value(new MimeType(exec, m));
                return true;
            }
        }
    }

    return PluginBase::getOwnProperty(exec, propertyName, result);
}


/************************************************************************/

bool Plugin::getOwnProperty(ExecState *exec, const Identifier& propertyName, Value& result) const
{
    // FIXME: should use the hashtable mechanism for properties

    if (propertyName == "name") {
        result = String(m_info->name);
        return true;
    } else if (propertyName == "filename") {
        result = String(m_info->file);
        return true;
    } else if (propertyName == "description") {
        result = String(m_info->desc);
        return true;
    } else if (propertyName == lengthPropertyName) {
        result = Number(m_info->mimes.count());
        return true;
    } else {
        // plugin[#]
        bool ok;
        unsigned int i = propertyName.toULong(&ok);
        if (ok && i < m_info->mimes.count()) {
            result = Value(new MimeType(exec, m_info->mimes.at(i)));
            return result;
        }

        // plugin["name"]
        for (MimeClassInfo *m=m_info->mimes.first(); m; m = m_info->mimes.next()) {
            if (m->type==propertyName.qstring()) {
                result = Value(new MimeType(exec, m));
                return result;
            }
        }

    }

    return ObjectImp::getOwnProperty(exec, propertyName, result);
}


/*****************************************************************************/

bool MimeType::getOwnProperty(ExecState *exec, const Identifier& propertyName, Value& result) const
{
    // FIXME: should use hashtable mechanism

    if (propertyName == "type") {
        result = String(m_info->type);
        return true;
    } else if (propertyName == "suffixes") {
        result = String(m_info->suffixes);
        return true;
    } else if (propertyName == "description") {
        result = String(m_info->desc);
        return true;
    } else if (propertyName == "enabledPlugin") {
        result = Value(new Plugin(exec, m_info->plugin));
        return true;
    }

    return ObjectImp::getOwnProperty(exec, propertyName, result);
}


Value PluginsFunc::call(ExecState *exec, Object &, const List &args)
{
    PluginBase(exec).refresh(args[0].toBoolean(exec));
    return Undefined();
}


Value NavigatorFunc::call(ExecState *exec, Object &thisObj, const List &)
{
  if (!thisObj.inherits(&KJS::Navigator::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  Navigator *nav = static_cast<Navigator *>(thisObj.imp());
  // javaEnabled()
  return Boolean(nav->part()->javaEnabled());
}

} // namespace
