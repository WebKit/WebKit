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
 
#import <Foundation/Foundation.h>
#import <WCPluginWidget.h>
#import <WCPluginDatabase.h>
#import <KWQView.h>
#include <kwqdebug.h>

@interface IFPluginView : NSObject
- initWithFrame: (NSRect) r plugin: (WCPlugin *)plug url: (NSString *)location mime:(NSString *)mime arguments:(NSDictionary *)arguments mode:(uint16)mode;
@end


static WCIFPluginMakeFunc WCIFPluginMake = NULL;

void WCSetIFPluginMakeFunc(WCIFPluginMakeFunc func)
{
    WCIFPluginMake = func;
}


WCPluginWidget::WCPluginWidget(const QString &url, const QString &serviceType, const QStringList &args)
{
    NSMutableDictionary *arguments;
    NSString *arg, *mimeType, *URL;
    NSRange r1, r2, r3;
    WCPlugin *plugin;
    uint i;
    
    URL = QSTRING_TO_NSSTRING(url);
    arguments = [NSMutableDictionary dictionaryWithCapacity:10];
    for(i=0; i<args.count(); i++){
        if(!args[i].contains("__KHTML__")){
            arg = QSTRING_TO_NSSTRING(args[i]);
            r1 = [arg rangeOfString:@"="]; // parse out attributes and values
            r2 = [arg rangeOfString:@"\""];
            r3.location = r2.location + 1;
            r3.length = [arg length] - r2.location - 2; // don't include quotes
            [arguments setObject:[arg substringWithRange:r3] forKey:[arg substringToIndex:r1.location]];
        }
    }
    
    if(serviceType.isNull()){
        plugin = [[WCPluginDatabase installedPlugins] getPluginForExtension:[URL pathExtension]];
        if(plugin != nil){
            mimeType = [plugin mimeTypeForURL:URL];
        }
    }else{
        plugin = [[WCPluginDatabase installedPlugins] getPluginForMimeType:QSTRING_TO_NSSTRING(serviceType)];
        mimeType = QSTRING_TO_NSSTRING(serviceType);
    }
    if(plugin == nil){
        //FIXME: Error dialog should be shown here
        printf("Could not find plugin for mime: %s or URL: %s\n", serviceType.latin1(), url.latin1());
        return;
    }
    setView(WCIFPluginMake(NSMakeRect(0,0,0,0), plugin, URL, mimeType, arguments, NP_EMBED));
}

WCPluginWidget::~WCPluginWidget()
{

}

void * WCIFPluginMakeFunction()
{
    return WCIFPluginMake;
}



