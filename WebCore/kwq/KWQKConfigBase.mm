/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <kwqdebug.h>
#include <kconfig.h>
#import <WCPlugin.h>
#import <WCPluginDatabase.h>

enum files{
    pluginsinfo
};

int file;
unsigned group;

//FIX ME:
static QString *tempQString = NULL;
static QColor *tempQColor = NULL;
static QStringList *tempQStringList = NULL;


KConfigBase::KConfigBase()
{
    _logNotYetImplemented();
}


KConfigBase::~KConfigBase()
{
    _logNotYetImplemented();
}


void KConfigBase::setGroup(const QString &pGroup)
{
    _logPartiallyImplemented();
    
    if(file == pluginsinfo){
        group = pGroup.toUInt();
    }
}


void KConfigBase::writeEntry(const QString &pKey, const QStringList &rValue, 
    char sep=',', bool bPersistent=true, bool bGlobal=false, 
    bool bNLS=false)
{
    _logNotYetImplemented();
}



QString KConfigBase::readEntry(const char *pKey, 
    const QString& aDefault=QString::null) const
{
    _logPartiallyImplemented();
    
    if(file == pluginsinfo){
        WCPlugin *plugin;
        NSArray *mimeTypes;
        NSMutableString *bigMimeString;
        NSString *bigMimeString2;
        uint i;
        
        plugin = [[[WCPluginDatabase installedPlugins] plugins] objectAtIndex:group];
        if(strcmp(pKey, "name") == 0){
            return NSSTRING_TO_QSTRING([plugin name]);
        }else if(strcmp(pKey, "file") == 0){
            return NSSTRING_TO_QSTRING([plugin filename]);
        }else if(strcmp(pKey, "description") == 0){
            return NSSTRING_TO_QSTRING([plugin pluginDescription]);
        }else if(strcmp(pKey, "mime") == 0){
            mimeTypes = [plugin mimeTypes];
            bigMimeString = [NSMutableString stringWithCapacity:1000];
            for(i=0; i<[mimeTypes count]; i++){
                [bigMimeString appendString:[[mimeTypes objectAtIndex:i] objectAtIndex:0]]; // mime type
                [bigMimeString appendString:@":"];
                [bigMimeString appendString:[[mimeTypes objectAtIndex:i] objectAtIndex:1]]; // mime's extension
                [bigMimeString appendString:@":"];
                [bigMimeString appendString:[[mimeTypes objectAtIndex:i] objectAtIndex:2]]; // mime's description
                [bigMimeString appendString:@";"];
            }
            bigMimeString2 = [NSString stringWithString:bigMimeString];
            [bigMimeString2 retain];
            return NSSTRING_TO_QSTRING(bigMimeString2);
        }
    }
    if(tempQString == NULL) {
        tempQString = new QString();
    }
    return *tempQString;
}



int KConfigBase::readNumEntry(const char *pKey, int nDefault=0) const
{
    _logPartiallyImplemented();
    
    if(file == pluginsinfo){
        return [[[WCPluginDatabase installedPlugins] plugins] count];
    }
    return 0;
}



unsigned int KConfigBase::readUnsignedNumEntry(const char *pKey, 
    unsigned int nDefault=0) const
{
    _logNotYetImplemented();
    return 0;
}


bool KConfigBase::readBoolEntry(const char *pKey, bool nDefault=0) const
{
    _logNotYetImplemented();
    return FALSE;
}


QColor KConfigBase::readColorEntry(const char *pKey, const QColor *pDefault=0L) const
{
    _logNotYetImplemented();
    if (tempQColor == NULL) {
        tempQColor = new QColor(0,0,0);
    }
    return *tempQColor;
}


QStringList KConfigBase::readListEntry(const QString &pKey, char sep=',') const
{
    _logNotYetImplemented();
    if (tempQStringList == NULL) {
        tempQStringList = new QStringList();
    }
    return *tempQStringList;
}


// class KConfig ===============================================================

KConfig::KConfig(const QString &n, bool bReadOnly=false, bool bUseKDEGlobals = true)
{
    _logPartiallyImplemented();
    if(n.contains(pluginsinfo)){
        file = pluginsinfo;
    }
}


KConfig::~KConfig()
{
    _logNotYetImplemented();
}
