/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
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

#include <qvector.h>
#include <dom_string.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kglobal.h>

#include <kjs/types.h>
#include <kjs/function.h>
#include <kjs/operations.h>
#include <kurl.h>
#include <kstddirs.h>
#include <kglobal.h>
#include <kconfig.h>
#include <kdebug.h>

#include <kio/kprotocolmanager.h>
#include "kjs_navigator.h"
#include "khtml_part.h"


using namespace KJS;

namespace KJS {

    class PluginBase : public HostImp {
    public:
        PluginBase();
        virtual ~PluginBase();

        struct MimeTypeInfo;
        struct PluginInfo;

        struct MimeTypeInfo {
            QString type;
            QString desc;
            QString suffixes;
            PluginInfo *plugin;
        };

        struct PluginInfo {
            QString name;
            QString file;
            QString desc;
            QList<MimeTypeInfo> mimes;
        };

        static QList<PluginInfo> *plugins;
        static QList<MimeTypeInfo> *mimes;

    private:
        static int m_refCount;
    };


    class Plugins : public PluginBase {
    public:
        Plugins() {};
        virtual KJSO get(const UString &p) const;
        virtual const TypeInfo* typeInfo() const { return &info; }
        static const TypeInfo info;
    private:
    };
    const TypeInfo Plugins::info = { "PluginArray", HostType, 0, 0, 0 };


    class MimeTypes : public PluginBase {
    public:
        MimeTypes() { };
        virtual KJSO get(const UString &p) const;
        virtual const TypeInfo* typeInfo() const { return &info; }
        static const TypeInfo info;
    private:
    };
    const TypeInfo MimeTypes::info = { "MimeTypeArray", HostType, 0, 0, 0 };


    class Plugin : public HostImp {
    public:
        Plugin( PluginBase::PluginInfo *info ) { m_info = info; };
        virtual KJSO get(const UString &p) const;
        virtual const TypeInfo* typeInfo() const { return &info; }
        static const TypeInfo info;
    private:
        PluginBase::PluginInfo *m_info;
    };
    const TypeInfo Plugin::info = { "Plugin", HostType, 0, 0, 0 };


    class MimeType : public HostImp {
    public:
        MimeType( PluginBase::MimeTypeInfo *info ) { m_info = info; };
        virtual KJSO get(const UString &p) const;
        virtual const TypeInfo* typeInfo() const { return &info; }
        static const TypeInfo info;
    private:
        PluginBase::MimeTypeInfo *m_info;
    };
    const TypeInfo MimeType::info = { "MimeType", HostType, 0, 0, 0 };

    class PluginsFunc : public DOMFunction {
    public:
        PluginsFunc() { };
        Completion tryExecute(const List &);
    };


    class NavigatorFunc : public InternalFunctionImp {
    public:
        NavigatorFunc(KHTMLPart *p) : part(p) { }
        Completion execute(const List &);

    private:
        KHTMLPart *part;
    };
};


QList<PluginBase::PluginInfo> *KJS::PluginBase::plugins = 0;
QList<PluginBase::MimeTypeInfo> *KJS::PluginBase::mimes = 0;
int KJS::PluginBase::m_refCount = 0;

bool Navigator::hasProperty(const UString &p, bool recursive) const
{
  if (p == "javaEnabled" ||
      p == "appCodeName" ||
      p == "appName" ||
      p == "appVersion" ||
      p == "language" ||
      p == "userAgent" ||
      p == "platform" ||
      p == "plugins" ||
      p == "mimeTypes" ||
      HostImp::hasProperty(p, recursive) )
    return true;
  return false;
}

KJSO Navigator::get(const UString &p) const
{
  KURL url = part->url();
  QString userAgent = KProtocolManager::userAgentForHost(url.host());

  if (p == "javaEnabled")
     return Function (new NavigatorFunc(part));
  else if (p == "appCodeName")
    return String("Mozilla");
  else if (p == "appName") {
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
    //kdDebug() << "appName -> Konqueror" << endl;
    return String("Konqueror");
  } else if (p == "appVersion"){
    // We assume the string is something like Mozilla/version (properties)
    return String(userAgent.mid(userAgent.find('/') + 1));
  } else if (p == "product") {
      return String("Konqueror/khtml");
  } else if (p == "vendor") {
      return String("KDE");
  } else if (p == "language") {
    return String(KGlobal::locale()->language() == "C" ?
                  QString::fromLatin1("en") : KGlobal::locale()->language());
  } else if (p == "userAgent") {
    return String(userAgent);
  } else if (p == "platform") {
    // yet another evil hack, but necessary to spoof some sites...
    if ( (userAgent.find(QString::fromLatin1("Win"),0,false)>=0) )
      return String(QString::fromLatin1("Win32"));
    else if ( (userAgent.find(QString::fromLatin1("Macintosh"),0,false)>=0) ||
              (userAgent.find(QString::fromLatin1("Mac_PowerPC"),0,false)>=0) )
      return String(QString::fromLatin1("MacPPC"));
    else
      return String(QString::fromLatin1("X11"));
  } else if (p == "plugins") {
      return KJSO(new Plugins());
  } else if (p == "mimeTypes") {
      return KJSO(new MimeTypes());
  } else
    return HostImp::get(p);
}


String Navigator::toString() const
{
  return UString("[object Navigator]");
}

/*******************************************************************/

PluginBase::PluginBase()
{
    if ( !plugins ) {
        plugins = new QList<PluginInfo>;
        mimes = new QList<MimeTypeInfo>;
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
                MimeTypeInfo *mime = new MimeTypeInfo;
                QStringList tokens = QStringList::split(':', *type, TRUE);
                QStringList::Iterator token;

                token = tokens.begin();
                mime->type = (*token).lower();
                //kdDebug(6070) << "mime->type=" << mime->type << endl;
                ++token;

                mime->suffixes = *token;
                ++token;

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


/*******************************************************************/


KJSO Plugins::get(const UString &p) const
{
    if (p == "refresh")
        return Function(new PluginsFunc());
    else if( p=="length" )
        return Number(plugins->count());
    else {

        // plugins[#]
        bool ok;
        unsigned int i = p.toULong(&ok);
        if( ok && i<plugins->count() )
            return KJSO( new Plugin( plugins->at(i) ) );

        // plugin[name]
        for ( PluginInfo *pl = plugins->first(); pl!=0; pl = plugins->next() ) {
            if ( pl->name==p.string() )
                return KJSO( new Plugin( pl ) );
        }
    }

    return PluginBase::get(p);
}

/*******************************************************************/


KJSO MimeTypes::get(const UString &p) const
{
    if( p=="length" )
        return Number( mimes->count() );
    else {

        // mimeTypes[#]
        bool ok;
        unsigned int i = p.toULong(&ok);
        if( ok && i<mimes->count() )
            return KJSO( new MimeType( mimes->at(i) ) );

        // mimeTypes[name]
        //kdDebug(6070) << "MimeTypes[" << p.ascii() << "]" << endl;
        for ( MimeTypeInfo *m=mimes->first(); m!=0; m=mimes->next() ) {
            //kdDebug(6070) << "m->type=" << m->type.ascii() << endl;
            if ( m->type==p.string() )
                return KJSO( new MimeType( m ) );
        }
    }

    return PluginBase::get(p);
}


/************************************************************************/


KJSO Plugin::get(const UString &p) const
{
    if ( p=="name" )
        return String( m_info->name );
    else if ( p=="filename" )
        return String( m_info->file );
    else if ( p=="description" )
        return String( m_info->desc );
    else if ( p=="length" )
        return Number( m_info->mimes.count() );
    else {

        // plugin[#]
        bool ok;
        unsigned int i = p.toULong(&ok);
        //kdDebug(6070) << "Plugin::get plugin[" << i << "]" << endl;
        if( ok && i<m_info->mimes.count() )
        {
            //kdDebug(6070) << "returning mimetype " << m_info->mimes.at(i)->type << endl;
            return KJSO( new MimeType( m_info->mimes.at(i) ) );
        }

        // plugin["name"]
        for ( PluginBase::MimeTypeInfo *m=m_info->mimes.first();
              m!=0; m=m_info->mimes.next() ) {
            if ( m->type==p.string() )
                return KJSO( new MimeType( m ) );
        }

    }

    return HostImp::get(p);
}


/*****************************************************************************/


KJSO MimeType::get(const UString &p) const
{
    if ( p=="type" )
        return String( m_info->type );
    else if ( p=="suffixes" )
        return String( m_info->suffixes );
    else if ( p=="description" )
        return String( m_info->desc );
    else if ( p=="enabledPlugin" )
        return KJSO( new Plugin( m_info->plugin ) );

    return HostImp::get(p);
}


Completion PluginsFunc::tryExecute(const List &)
{
  return Completion(Normal);
}


Completion NavigatorFunc::execute(const List &)
{
  // javaEnabled()
  return Completion(ReturnValue, Boolean(part->javaEnabled()));
}
