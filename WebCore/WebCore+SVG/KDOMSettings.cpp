/*
    Copyright (C) 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#define DEBUG_SETTINGS 0

#include "config.h"
#if SVG_SUPPORT
#include <kdebug.h>
#include <kconfig.h>
#include <kglobal.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kglobalsettings.h>

#include <qregexp.h>
#include <qfontdatabase.h>
#include <kxmlcore/Vector.h>

#include "KDOMSettings.h"

using namespace KDOM;

QColor txtDefault(DOM_DEFAULT_TXT_COLOR);
QColor lnkDefault(DOM_DEFAULT_LNK_COLOR);
QColor vlnkDefault(DOM_DEFAULT_VLNK_COLOR);
QColor baseDefault(DOM_DEFAULT_BASE_COLOR);

/**
 * @internal
 * Contains all settings which are both available globally and per-domain
 */
struct KPerDomainSettings
{
    bool enableJava : 1;
    bool enableJavaScript : 1;
    bool enablePlugins : 1;

    // don't forget to maintain the bitfields as the enums grow
    KDOMSettings::KJSWindowOpenPolicy windowOpenPolicy : 2;
    KDOMSettings::KJSWindowStatusPolicy windowStatusPolicy : 1;
    KDOMSettings::KJSWindowFocusPolicy windowFocusPolicy : 1;
    KDOMSettings::KJSWindowMovePolicy windowMovePolicy : 1;
    KDOMSettings::KJSWindowResizePolicy windowResizePolicy : 1;

#if DEBUG_SETTINGS > 0
    void dump(const QString &infix = QString::null) const
    {
        kdDebug() << "KPerDomainSettings " << infix << " @" << this << ":" << endl;
        kdDebug() << "  enableJava: " << enableJava << endl;
        kdDebug() << "  enableJavaScript: " << enableJavaScript << endl;
        kdDebug() << "  enablePlugins: " << enablePlugins << endl;
        kdDebug() << "  windowOpenPolicy: " << windowOpenPolicy << endl;
        kdDebug() << "  windowStatusPolicy: " << windowStatusPolicy << endl;
        kdDebug() << "  windowFocusPolicy: " << windowFocusPolicy << endl;
        kdDebug() << "  windowMovePolicy: " << windowMovePolicy << endl;
        kdDebug() << "  windowResizePolicy: " << windowResizePolicy << endl;
    }
#endif
};

typedef QMap<QString, KPerDomainSettings> PolicyMap;

class KDOMSettings::Private
{
public:
    QStringList fonts;
    QStringList defaultFonts;
    
    int fontSize;
    int minFontSize;

    QColor textColor;
    QColor baseColor;
    QColor linkColor;
    QColor vLinkColor;

    // the virtual global "domain"
    KPerDomainSettings global;

    KDOMSettings::KAnimationAdvice showAnimations;

    QString encoding;
    QString userSheet;

    PolicyMap domainPolicy;

    Vector<QRegExp> adFilters;
    Q3ValueList< QPair< QString, QChar > > fallbackAccessKeysAssignments;

    // Flags
    bool enforceCharset : 1;
    bool followSystemColors : 1;

    bool hideAdsEnabled : 1;
    bool adFilterEnabled : 1;

    bool jsErrorsEnabled : 1;
    bool jsPopupBlockerPassivePopup : 1;

    bool enableJavaScriptDebug : 1;
    bool enableJavaScriptErrorReporting : 1;
};

/**
 * Local helper for retrieving per-domain settings.
 * In case of doubt, the global domain is returned.
 */
static const KPerDomainSettings &lookup_hostname_policy(PolicyMap &policies,
                                                        const KPerDomainSettings &global,
                                                        const QString &hostname)
{
#if DEBUG_SETTINGS > 0
    kdDebug() << "lookup_hostname_policy(" << hostname << ")" << endl;
#endif

    if(hostname.isEmpty())
    {
#if DEBUG_SETTINGS > 0
        global.dump("global");
#endif

        return global;
    }

    const PolicyMap::const_iterator notfound = policies.end();

    // First check whether there is a perfect match.
    PolicyMap::const_iterator it = policies.find(hostname);
    if(it != notfound)
    {
#if DEBUG_SETTINGS > 0
        kdDebug() << "perfect match" << endl;
        (*it).dump(hostname);
#endif

        // yes, use it (unless dunno)
        return *it;
    }

    // Now, check for partial match.  Chop host from the left until
    // there's no dots left.
    QString host_part = hostname;
    int dot_idx = -1;
    while((dot_idx = host_part.find(QChar('.'))) >= 0)
    {
        host_part.remove(0,dot_idx);
        it = policies.find(host_part);
        
        ASSERT(notfound == policies.end());
        if (it != notfound) {
#if DEBUG_SETTINGS > 0
            kdDebug() << "partial match" << endl;
            (*it).dump(host_part);
#endif

            return *it;
        }
        
        // ASSERT(host_part[0] == QChar('.'));
        host_part.remove(0,1); // Chop off the dot.
    }

    // No domain-specific entry: use global domain
#if DEBUG_SETTINGS > 0
    kdDebug() << "no match" << endl;
    global.dump("global");
#endif

    return global;
}

KDOMSettings::KDOMSettings() : d(new Private())
{
    init();
}

KDOMSettings::KDOMSettings(const KDOMSettings &other) : d(new Private())
{
    *d = *other.d;
}

KDOMSettings::~KDOMSettings()
{
    delete d;
}

void KDOMSettings::init()
{
    KConfig global(QString::fromLatin1("kdomrc"), true, false);
    init(&global, true);

    KConfig *local = KGlobal::config();
    if(!local)
        return;

    init(local, false);
}

void KDOMSettings::init(KConfig *config, bool reset)
{
#ifndef APPLE_COMPILE_HACK
    QString group_save = config->group();

    if(reset || config->hasGroup("DOM Settings"))
    {
        config->setGroup("DOM Settings");
    
        // Fonts and colors
        if(reset)
        {
            d->defaultFonts = QStringList();
            d->defaultFonts.append(config->readEntry(QString::fromLatin1("StandardFont"), KGlobalSettings::generalFont().family()));
            d->defaultFonts.append(config->readEntry(QString::fromLatin1("FixedFont"), KGlobalSettings::fixedFont().family()));
            d->defaultFonts.append(config->readEntry(QString::fromLatin1("SerifFont"), DOM_DEFAULT_VIEW_SERIF_FONT));
            d->defaultFonts.append(config->readEntry(QString::fromLatin1("SansSerifFont"), DOM_DEFAULT_VIEW_SANSSERIF_FONT));
            d->defaultFonts.append(config->readEntry(QString::fromLatin1("CursiveFont"), DOM_DEFAULT_VIEW_CURSIVE_FONT));
            d->defaultFonts.append(config->readEntry(QString::fromLatin1("FantasyFont"), DOM_DEFAULT_VIEW_FANTASY_FONT));
            d->defaultFonts.append(QString::fromLatin1("0")); // font size adjustment
        }

        if(reset || config->hasKey("MinimumFontSize"))
            d->minFontSize = config->readNumEntry(QString::fromLatin1("MinimumFontSize"), DOM_DEFAULT_MIN_FONT_SIZE);

        if(reset || config->hasKey("MediumFontSize"))
            d->fontSize = config->readNumEntry(QString::fromLatin1("MediumFontSize"), 12);

        d->fonts = config->readListEntry("Fonts");

        if(reset || config->hasKey("DefaultEncoding"))
            d->encoding = config->readEntry(QString::fromLatin1("DefaultEncoding"), QString::fromLatin1(""));

        if(reset || config->hasKey("EnforceDefaultCharset"))
            d->enforceCharset = config->readBoolEntry(QString::fromLatin1("EnforceDefaultCharset"), false);

        if(reset || config->hasKey("ShowAnimations"))
        {
            QString value = config->readEntry(QString::fromLatin1("ShowAnimations")).lower();
            if(value == QString::fromLatin1("disabled"))
                d->showAnimations = KAnimationDisabled;
            else if(value == QString::fromLatin1("looponce"))
                d->showAnimations = KAnimationLoopOnce;
            else
                d->showAnimations = KAnimationEnabled;
        }

        if(config->readBoolEntry("UserStyleSheetEnabled", false) == true)
        {
            if(reset || config->hasKey("UserStyleSheet"))
                d->userSheet = config->readEntry(QString::fromLatin1("UserStyleSheet"), QString::fromLatin1(""));
        }
    }

    if(reset || config->hasKey("FollowSystemColors"))
        d->followSystemColors = config->readBoolEntry( "FollowSystemColors", false);

    if(reset || config->hasGroup("General"))
    {
        config->setGroup("General"); // group will be restored by cgs anyway
        if(reset || config->hasKey("foreground"))
            d->textColor = config->readColorEntry("foreground", &txtDefault);

        if(reset || config->hasKey("linkColor"))
            d->linkColor = config->readColorEntry("linkColor", &lnkDefault);

        if(reset || config->hasKey("visitedLinkColor"))
            d->vLinkColor = config->readColorEntry("visitedLinkColor", &vlnkDefault);

        if(reset || config->hasKey("background"))
            d->baseColor = config->readColorEntry("background", &baseDefault);
    }

    if(reset || config->hasGroup("Java/JavaScript Settings"))
    {
        config->setGroup("Java/JavaScript Settings");

        // The global setting for JavaScript debugging
        // This is currently always enabled by default
        if(reset || config->hasKey("EnableJavaScriptDebug"))
            d->enableJavaScriptDebug = config->readBoolEntry("EnableJavaScriptDebug", false);

        // The global setting for JavaScript error reporting
        if(reset || config->hasKey( "ReportJavaScriptErrors"))
            d->enableJavaScriptErrorReporting = config->readBoolEntry("ReportJavaScriptErrors", false);

        // The global setting for popup block passive popup
        if(reset || config->hasKey( "PopupBlockerPassivePopup"))
            d->jsPopupBlockerPassivePopup = config->readBoolEntry("PopupBlockerPassivePopup", true);

        // Read options from the global "domain"
        readDomainSettings(config, reset, true, d->global);

#if DEBUG_SETTINGS > 0
        d->global.dump("init global");
#endif

        // The domain-specific settings. (always keep the order of these keys!)
        static const char *const domain_keys[] = { "ECMADomains", "JavaDomains", "PluginDomains" };

        bool check_old_ecma_settings = true;
        bool check_old_java_settings = true;

        // merge all domains into one list
        QMap<QString, int> domainList;   // why can't Qt have a QSet?
        for(unsigned i = 0; i < (sizeof(domain_keys) / sizeof(domain_keys[0])); ++i)
        {
            if(reset || config->hasKey(domain_keys[i]))
            {
                if(i == 0)
                    check_old_ecma_settings = false;
                else if(i == 1)
                    check_old_java_settings = false;

                const QStringList dl = config->readListEntry(domain_keys[i]);
                const QMap<QString, int>::Iterator notfound = domainList.end();
                QStringList::ConstIterator it = dl.begin();
                const QStringList::ConstIterator itEnd = dl.end();
                for(; it != itEnd; ++it)
                {
                    const QString domain = (*it).lower();
                    QMap<QString, int>::Iterator pos = domainList.find(domain);
                    if(pos == notfound)
                        domainList.insert(domain, 0);
                }
            }
        }

        if(reset)
            d->domainPolicy.clear();

        QString js_group_save = config->group();

        {
            QMap<QString, int>::ConstIterator it = domainList.begin();
            const QMap<QString,int>::ConstIterator itEnd = domainList.end();
            for( ; it != itEnd; ++it)
            {
                const QString domain = it.key();
                config->setGroup(domain);
                readDomainSettings(config, reset, false, d->domainPolicy[domain]);

#if DEBUG_SETTINGS > 0
                d->domainPolicy[domain].dump("init " + domain);
#endif
            }
        }

        config->setGroup(js_group_save);

        bool check_old_java = true;
        if((reset || config->hasKey( "JavaDomainSettings")) && check_old_java_settings)
        {
            check_old_java = false;
            const QStringList domainList = config->readListEntry( "JavaDomainSettings" );
            QStringList::ConstIterator it = domainList.begin();
            const QStringList::ConstIterator itEnd = domainList.end();
            for(; it != itEnd; ++it)
            {
                QString domain;
                KJavaScriptAdvice javaAdvice;
                KJavaScriptAdvice javaScriptAdvice;
                splitDomainAdvice(*it, domain, javaAdvice, javaScriptAdvice);
                setup_per_domain_policy(d->domainPolicy, d->global, domain).enableJava = (javaAdvice == KJavaScriptAccept);

#if DEBUG_SETTINGS > 0
                setup_per_domain_policy(d->domainPolicy, d->global, domain).dump("JavaDomainSettings 4 " + domain);
#endif
            }
        }

        bool check_old_ecma = true;
        if(( reset || config->hasKey("ECMADomainSettings")) && check_old_ecma_settings)
        {
            check_old_ecma = false;
            const QStringList domainList = config->readListEntry( "ECMADomainSettings" );
            QStringList::ConstIterator it = domainList.begin();
            const QStringList::ConstIterator itEnd = domainList.end();
            for(; it != itEnd; ++it)
            {
                QString domain;
                KJavaScriptAdvice javaAdvice;
                KJavaScriptAdvice javaScriptAdvice;
                splitDomainAdvice(*it, domain, javaAdvice, javaScriptAdvice);
                setup_per_domain_policy(d->domainPolicy, d->global, domain).enableJavaScript = (javaScriptAdvice == KJavaScriptAccept);

#if DEBUG_SETTINGS > 0
                setup_per_domain_policy(d->domainPolicy, d->global, domain).dump("ECMADomainSettings 4 " + domain);
#endif
            }
        }

        if((reset || config->hasKey("JavaScriptDomainAdvice")) &&
           (check_old_java || check_old_ecma) &&
           (check_old_ecma_settings || check_old_java_settings))
        {
            const QStringList domainList = config->readListEntry( "JavaScriptDomainAdvice" );
            QStringList::ConstIterator it = domainList.begin();
            const QStringList::ConstIterator itEnd = domainList.end();
            for( ; it != itEnd; ++it)
            {
                QString domain;
                KJavaScriptAdvice javaAdvice;
                KJavaScriptAdvice javaScriptAdvice;
                splitDomainAdvice(*it, domain, javaAdvice, javaScriptAdvice);

                if(check_old_java)
                    setup_per_domain_policy(d->domainPolicy, d->global, domain).enableJava = (javaAdvice == KJavaScriptAccept);

                if(check_old_ecma)
                    setup_per_domain_policy(d->domainPolicy, d->global, domain).enableJavaScript = (javaScriptAdvice == KJavaScriptAccept);

#if DEBUG_SETTINGS > 0
                setup_per_domain_policy(d->domainPolicy, d->global, domain).dump("JavaScriptDomainAdvice 4 " + domain);
#endif
            }
        }
    }

    if(reset || config->hasGroup("Filter Settings"))
    {
        config->setGroup("Filter Settings");
        d->adFilterEnabled = config->readBoolEntry("Enabled", false);
        d->hideAdsEnabled = config->readBoolEntry("Shrink", false);

        d->adFilters.clear();

        QMap<QString,QString> entryMap = config->entryMap(QString::fromLatin1("Filter Settings"));
        QMap<QString,QString>::ConstIterator it;
        d->adFilters.reserveCapacity(entryMap.count());
        for( it = entryMap.constBegin(); it != entryMap.constEnd(); ++it)
        {
            QString name = it.key();
            QString value = it.data();

            if(value.startsWith(QString::fromLatin1("!")))
                continue;

            if(name.startsWith(QString::fromLatin1("Filter")))
            {
                if(value.length() > 2 && value[0] == '/' && value[value.length() - 1] == '/')
                {
                    QString inside = value.mid(1, value.length()-2);
                    QRegExp rx(inside);
                    d->adFilters.append(rx);
                }
                else
                {
                    QRegExp rx(value);
                    rx.setWildcard(true);
                    d->adFilters.append(rx);
                }
            }
        }
    }

    config->setGroup(group_save);
#endif
}

KDOMSettings::KAnimationAdvice KDOMSettings::showAnimations() const
{
    return d->showAnimations;
}

KDOMSettings::KJSWindowOpenPolicy KDOMSettings::windowOpenPolicy(const QString &hostname) const
{
    return lookup_hostname_policy(d->domainPolicy, d->global, hostname.lower()).windowOpenPolicy;
}

KDOMSettings::KJSWindowMovePolicy KDOMSettings::windowMovePolicy(const QString &hostname) const
{
    return lookup_hostname_policy(d->domainPolicy, d->global, hostname.lower()).windowMovePolicy;
}

KDOMSettings::KJSWindowResizePolicy KDOMSettings::windowResizePolicy(const QString &hostname) const
{
    return lookup_hostname_policy(d->domainPolicy, d->global, hostname.lower()).windowResizePolicy;
}

KDOMSettings::KJSWindowStatusPolicy KDOMSettings::windowStatusPolicy(const QString &hostname) const
{
    return lookup_hostname_policy(d->domainPolicy, d->global, hostname.lower()).windowStatusPolicy;
}

KDOMSettings::KJSWindowFocusPolicy KDOMSettings::windowFocusPolicy(const QString &hostname) const
{
    return lookup_hostname_policy(d->domainPolicy, d->global, hostname.lower()).windowFocusPolicy;
}

QString KDOMSettings::stdFontName() const
{
    return lookupFont(0);
}

QString KDOMSettings::fixedFontName() const
{
    return lookupFont(1);
}

QString KDOMSettings::serifFontName() const
{
    return lookupFont(2);
}

QString KDOMSettings::sansSerifFontName() const
{
    return lookupFont(3);
}

QString KDOMSettings::cursiveFontName() const
{
    return lookupFont(4);
}

QString KDOMSettings::fantasyFontName() const
{
    return lookupFont(5);
}

int KDOMSettings::minFontSize() const
{
    return d->minFontSize;
}

int KDOMSettings::mediumFontSize() const
{
    return d->fontSize;
}

const QString &KDOMSettings::encoding() const
{
    return d->encoding;
}

const QColor &KDOMSettings::baseColor() const
{
    return d->baseColor;
}

const QColor &KDOMSettings::linkColor() const
{
    return d->linkColor;
}

const QColor &KDOMSettings::vLinkColor() const
{
    return d->vLinkColor;
}

bool KDOMSettings::isJavaEnabled(const QString &hostname) const
{
    return lookup_hostname_policy(d->domainPolicy, d->global, hostname.lower()).enableJava;
}

bool KDOMSettings::isJavaScriptEnabled(const QString &hostname) const
{
    return lookup_hostname_policy(d->domainPolicy, d->global, hostname.lower()).enableJavaScript;
}

bool KDOMSettings::isJavaScriptDebugEnabled(const QString &/* hostname */) const
{
    // debug setting is global for now, but could change in the future
    return d->enableJavaScriptDebug;
}

bool KDOMSettings::isJavaScriptErrorReportingEnabled(const QString &/* hostname */) const
{
    // error reporting setting is global for now, but could change in the future
    return d->enableJavaScriptErrorReporting;
}

bool KDOMSettings::isPluginsEnabled(const QString &hostname) const
{
    return lookup_hostname_policy(d->domainPolicy, d->global, hostname.lower()).enablePlugins;
}

bool KDOMSettings::isAdFiltered(const QString &url) const
{
    if(d->adFilterEnabled)
    {
        if(!url.startsWith(QString::fromLatin1("data:")))
        {
            Vector<QRegExp>::iterator it;
            for(it = d->adFilters.begin(); it != d->adFilters.end(); ++it)
            {
                if(it->search(url) != -1)
                {
                    kdDebug(6080) << "Filtered: " << url << endl;
                    return true;
                }
            }
        }
    }

    return false;
}

bool KDOMSettings::isAdFilterEnabled() const
{
    return d->adFilterEnabled;
}
    
bool KDOMSettings::isHideAdsEnabled() const
{
    return d->hideAdsEnabled;
}

void KDOMSettings::addAdFilter(const QString &url)
{
    KConfig config(QString::fromLatin1("kdomrc"), false, false);
    config.setGroup("Filter Settings");

    QRegExp rx;
    if(url.length() > 2 && url[0] == '/' && url[url.length() - 1] == '/')
    {
        QString inside = url.mid(1, url.length() - 2);
        rx.setWildcard(false);
        rx.setPattern(inside);
    }
    else
    {
        rx.setWildcard(true);
        rx.setPattern(url);
    }

    if(rx.isValid())
    {
        int last = config.readNumEntry(QString::fromLatin1("Count"), 0);
        QString key = QString::fromLatin1("Filter-") + QString::number(last);
        config.writeEntry(key, url);
        config.writeEntry("Count",last+1);
        config.sync();

        d->adFilters.append(rx);
    }
    else
        KMessageBox::error(0, rx.errorString(), i18n("Filter error"));
}

bool KDOMSettings::jsPopupBlockerPassivePopup() const
{
    return d->jsPopupBlockerPassivePopup;
}

void KDOMSettings::setJSPopupBlockerPassivePopup(bool enabled)
{
    d->jsPopupBlockerPassivePopup = enabled;

    // save it
    KConfig *config = KGlobal::config();
    config->setGroup("Java/JavaScript Settings");
    config->writeEntry("PopupBlockerPassivePopup", enabled);
    config->sync();
}

bool KDOMSettings::jsErrorsEnabled() const
{
    return d->jsErrorsEnabled;
}

void KDOMSettings::setJSErrorsEnabled(bool enabled)
{
    d->jsErrorsEnabled = enabled;

    // save it
    KConfig *config = KGlobal::config();
    config->setGroup("DOM Settings");
    config->writeEntry("ReportJSErrors", enabled);
    config->sync();
}

KDOMSettings::KJavaScriptAdvice KDOMSettings::strToAdvice(const QString &str)
{
    KJavaScriptAdvice ret = KJavaScriptDunno;

    if(str.isNull())
        ret = KJavaScriptDunno;

    if(str.lower() == QString::fromLatin1("accept"))
        ret = KJavaScriptAccept;
    else if (str.lower() == QString::fromLatin1("reject"))
        ret = KJavaScriptReject;

    return ret;
}

void KDOMSettings::splitDomainAdvice(const QString &configStr, QString &domain,
                                     KJavaScriptAdvice &javaAdvice, KJavaScriptAdvice &javaScriptAdvice)
{
    QString tmp(configStr);
    int splitIndex = tmp.find(':');
    if(splitIndex == -1)
    {
        domain = configStr.lower();
        javaAdvice = KJavaScriptDunno;
        javaScriptAdvice = KJavaScriptDunno;
    }
    else
    {
        domain = tmp.left(splitIndex).lower();
        QString adviceString = tmp.mid(splitIndex + 1, tmp.length());
        int splitIndex2 = adviceString.find( ':' );
        if(splitIndex2 == -1)
        {
            // Java advice only
            javaAdvice = strToAdvice(adviceString);
            javaScriptAdvice = KJavaScriptDunno;
        }
        else
        {
            // Java and JavaScript advice
            javaAdvice = strToAdvice(adviceString.left(splitIndex2));
            javaScriptAdvice = strToAdvice(adviceString.mid(splitIndex2 + 1, adviceString.length()));
        }
    }
}

const char *KDOMSettings::adviceToStr(KJavaScriptAdvice _advice)
{
    switch(_advice)
    {
        case KJavaScriptAccept:
            return I18N_NOOP("Accept");
        case KJavaScriptReject:
            return I18N_NOOP("Reject");
        default:
            return 0;
    }

    return 0;
}

void KDOMSettings::readDomainSettings(KConfig *config, bool reset, bool global, KPerDomainSettings &pd_settings)
{
    QString jsPrefix = global ? QString::null : QString::fromLatin1("javascript.");
    QString javaPrefix = global ? QString::null : QString::fromLatin1("java.");
    QString pluginsPrefix = global ? QString::null : QString::fromLatin1("plugins.");

    // The setting for Java
    QString key = javaPrefix + QString::fromLatin1("EnableJava");
    if((global && reset) || config->hasKey(key))
        pd_settings.enableJava = config->readBoolEntry(key, false);
    else if(!global)
        pd_settings.enableJava = d->global.enableJava;

    // The setting for Plugins
    key = pluginsPrefix + QString::fromLatin1("EnablePlugins");
    if((global && reset) || config->hasKey(key))
        pd_settings.enablePlugins = config->readBoolEntry(key, true);
    else if(!global)
        pd_settings.enablePlugins = d->global.enablePlugins;

    // The setting for JavaScript
    key = jsPrefix + QString::fromLatin1("EnableJavaScript");
    if((global && reset) || config->hasKey(key))
        pd_settings.enableJavaScript = config->readBoolEntry(key, true);
    else if(!global)
        pd_settings.enableJavaScript = d->global.enableJavaScript;

    // window property policies
    key = jsPrefix + QString::fromLatin1("WindowOpenPolicy");
    if((global && reset) || config->hasKey(key))
        pd_settings.windowOpenPolicy = (KJSWindowOpenPolicy) config->readUnsignedNumEntry(key, KJSWindowOpenAllow);
    else if(!global)
        pd_settings.windowOpenPolicy = d->global.windowOpenPolicy;

    key = jsPrefix + QString::fromLatin1("WindowMovePolicy");
    if((global && reset) || config->hasKey(key))
        pd_settings.windowMovePolicy = (KJSWindowMovePolicy) config->readUnsignedNumEntry(key, KJSWindowMoveAllow);
    else if(!global)
        pd_settings.windowMovePolicy = d->global.windowMovePolicy;

    key = jsPrefix + QString::fromLatin1("WindowResizePolicy");
    if((global && reset) || config->hasKey(key))
        pd_settings.windowResizePolicy = (KJSWindowResizePolicy) config->readUnsignedNumEntry(key, KJSWindowResizeAllow);
    else if(!global)
        pd_settings.windowResizePolicy = d->global.windowResizePolicy;

    key = jsPrefix + QString::fromLatin1("WindowStatusPolicy");
    if((global && reset) || config->hasKey(key))
        pd_settings.windowStatusPolicy = (KJSWindowStatusPolicy) config->readUnsignedNumEntry(key, KJSWindowStatusAllow);
    else if(!global)
        pd_settings.windowStatusPolicy = d->global.windowStatusPolicy;

    key = jsPrefix + QString::fromLatin1("WindowFocusPolicy");
    if((global && reset) || config->hasKey(key))
        pd_settings.windowFocusPolicy = (KJSWindowFocusPolicy) config->readUnsignedNumEntry(key, KJSWindowFocusAllow);
    else if(!global)
        pd_settings.windowFocusPolicy = d->global.windowFocusPolicy;
}

QString KDOMSettings::settingsToCSS() const
{
    return QString::null;
}

QString KDOMSettings::userStyleSheet() const
{
    return d->userSheet;
}

QString KDOMSettings::lookupFont(unsigned int i) const
{
    QString font;
    if(d->fonts.count() > i)
        font = d->fonts[i];

    if(font.isEmpty())
        font = d->defaultFonts[i];

    return font;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

