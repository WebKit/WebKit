/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <kconfig.h>

#import <KWQLogging.h>
#import <qcolor.h>
#import <qstringlist.h>
#import <WebCoreSettings.h>
#import <WebCoreViewFactory.h>

class KWQKConfigImpl
{
public:
    bool isPluginInfo;
    bool isKonquerorRC;
    int pluginIndex;
};

KConfig::KConfig(const QString &n, bool bReadOnly, bool bUseKDEGlobals)
{
    impl = new KWQKConfigImpl;
    impl->isPluginInfo = n.contains("pluginsinfo");
    impl->isKonquerorRC = (n == "konquerorrc");
    impl->pluginIndex = 0;
}

KConfig::~KConfig()
{
    delete impl;
}

void KConfig::setGroup(const QString &pGroup)
{
    if (impl->isPluginInfo) {
        impl->pluginIndex = pGroup.toUInt();
    }
}

void KConfig::writeEntry(const QString &pKey, const QStringList &rValue, 
    char sep, bool bPersistent, bool bGlobal, bool bNLS)
{
    ERROR("not yet implemented");
}

QString KConfig::readEntry(const char *pKey, const QString& aDefault) const
{
    if (impl->isPluginInfo) {
        id <WebCorePluginInfo> plugin = [[[WebCoreViewFactory sharedFactory] pluginsInfo] objectAtIndex:impl->pluginIndex];
        if (strcmp(pKey, "name") == 0) {
            return QString::fromNSString([plugin name]);
        } else if (strcmp(pKey, "file") == 0) {
            return QString::fromNSString([plugin filename]);
        } else if (strcmp(pKey, "description") == 0) {
            return QString::fromNSString([plugin pluginDescription]);
        } else if (strcmp(pKey, "mime") == 0) {
            NSDictionary *MIMEToExtensions =  [plugin MIMEToExtensionsDictionary];
            NSDictionary *MIMEToDescription = [plugin MIMEToDescriptionDictionary];
            NSArray *MIMETypes = [MIMEToExtensions allKeys], *extensions;
            NSString *MIME, *extension;
            NSMutableString *bigMimeString = [NSMutableString string];
            
            for (uint i = 0; i < [MIMETypes count]; i++) {
                MIME = [MIMETypes objectAtIndex:i];
                [bigMimeString appendFormat:@"%@:", MIME]; // mime type

                extensions = [MIMEToExtensions objectForKey:MIME];
                for (uint n = 0; n < [extensions count]; n++) {
                    extension = [extensions objectAtIndex:n];
                    [bigMimeString appendString:extension]; // mime's extension
                    if(n < [extensions count] - 1){
                        [bigMimeString appendString:@","];
                    }
                }
                [bigMimeString appendFormat:@":%@;", [MIMEToDescription objectForKey:MIME]]; // mime's description
            }
            return QString::fromNSString(bigMimeString);
        }
    }
    
    ERROR("not yet implemented");
    return QString::null;
}

int KConfig::readNumEntry(const char *pKey, int nDefault) const
{
    if (impl->isPluginInfo && strcmp(pKey, "number") == 0) {
        return [[[WebCoreViewFactory sharedFactory] pluginsInfo] count];
    }
    ERROR("not yet implemented");
    return nDefault;
}

unsigned int KConfig::readUnsignedNumEntry(const char *pKey, unsigned int nDefault) const
{
    if (impl->isKonquerorRC && strcmp(pKey, "WindowOpenPolicy") == 0) {
	if ([[WebCoreSettings sharedSettings] JavaScriptCanOpenWindowsAutomatically]) {
	    return 0;
	} else {
	    return 3;
	}
    }
    ERROR("not yet implemented");
    return nDefault;
}

bool KConfig::readBoolEntry(const char *pKey, bool nDefault) const
{
    ERROR("not yet implemented");
    return nDefault;
}

QColor KConfig::readColorEntry(const char *pKey, const QColor *pDefault) const
{
    return pDefault ? *pDefault : QColor(0, 0, 0);
}

QStringList KConfig::readListEntry(const QString &pKey, char sep) const
{
    ERROR("not yet implemented");
    return QStringList();
}
