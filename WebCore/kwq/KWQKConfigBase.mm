/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#import "KWQKConfigBase.h"

#import "Color.h"
#import "KWQExceptions.h"
#import "KWQKHTMLSettings.h"
#import "KWQLogging.h"
#import "QStringList.h"
#import "WebCoreSettings.h"
#import "WebCoreViewFactory.h"

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

QString KConfig::readEntry(const char *pKey, const QString& aDefault) const
{
    if (impl->isPluginInfo) {
	KWQ_BLOCK_EXCEPTIONS;

	NSString *result = nil;

        id <WebCorePluginInfo> plugin = [[[WebCoreViewFactory sharedFactory] pluginsInfo] objectAtIndex:impl->pluginIndex];
        if (strcmp(pKey, "name") == 0) {
            result = [plugin name];
        } else if (strcmp(pKey, "file") == 0) {
            result = [plugin filename];
        } else if (strcmp(pKey, "description") == 0) {
            result = [plugin pluginDescription];
        } else if (strcmp(pKey, "mime") == 0) {
            NSEnumerator *MIMETypeEnumerator = [plugin MIMETypeEnumerator], *extensionEnumerator;
            NSMutableString *MIMEString = [NSMutableString string];
            NSString *MIME, *extension;
            NSArray *extensions;
            
            while ((MIME = [MIMETypeEnumerator nextObject]) != nil) {
                [MIMEString appendFormat:@"%@:", MIME];

                extensions = [plugin extensionsForMIMEType:MIME];
                extensionEnumerator = [extensions objectEnumerator];
                
                while ((extension = [extensionEnumerator nextObject]) != nil) {
                    [MIMEString appendFormat:@"%@,", extension];
                }
                // Delete the last ",".
                NSRange tempRange = { [MIMEString length]-1, 1 }; // workaround for 4213314
                [MIMEString deleteCharactersInRange:tempRange];
                [MIMEString appendFormat:@":%@;", [plugin descriptionForMIMEType:MIME]];
            }

            result = MIMEString;
        }

	return QString::fromNSString(result);

	KWQ_UNBLOCK_EXCEPTIONS;

	return QString();
    }
    
    LOG_ERROR("config %s not implemented", pKey);
    return QString();
}

int KConfig::readNumEntry(const char *pKey, int nDefault) const
{
    if (impl->isPluginInfo && strcmp(pKey, "number") == 0) {
        KWQ_BLOCK_EXCEPTIONS;
	return [[[WebCoreViewFactory sharedFactory] pluginsInfo] count];
	KWQ_UNBLOCK_EXCEPTIONS;
	
	return 0;
    }
    LOG_ERROR("config %s not implemented", pKey);
    return nDefault;
}

unsigned KConfig::readUnsignedNumEntry(const KHTMLSettings *settings, const char *pKey, unsigned nDefault) const
{
    if (impl->isKonquerorRC && strcmp(pKey, "WindowOpenPolicy") == 0) {
        if (settings->JavaScriptCanOpenWindowsAutomatically()) {
	    return 0;
	} else {
	    return 3;
	}
    }
    LOG_ERROR("config %s not implemented", pKey);
    return nDefault;
}

Color KConfig::readColorEntry(const char *pKey, const Color *pDefault) const
{
    return pDefault ? *pDefault : Color(0, 0, 0);
}

void RefreshPlugins(bool reload)
{
    KWQ_BLOCK_EXCEPTIONS;
    [[WebCoreViewFactory sharedFactory] refreshPlugins:reload];
    KWQ_UNBLOCK_EXCEPTIONS;
}

