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
#import <WCJavaAppletWidget.h>
#import <WCPluginDatabase.h>
#import <WCPlugin.h>
#import <KWQView.h>

#include <kwqdebug.h>
#include <WCPluginWidget.h>

@interface IFPluginView : NSObject
- initWithFrame: (NSRect) r plugin: (WCPlugin *)plug url: (NSString *)location mime:(NSString *)mime arguments:(NSDictionary *)arguments mode:(uint16)mode;
@end

WCIFPluginMakeFunc WCIFPluginMake;

WCJavaAppletWidget::WCJavaAppletWidget(QMap<QString, QString> args)
{
    NSMutableDictionary *arguments;
    WCPlugin *plugin;
    QMap<QString, QString>::Iterator it;

    WCIFPluginMake = WCIFPluginMakeFunction();

    plugin = [[WCPluginDatabase installedPlugins] getPluginForFilename:@"Java.plugin"];
    if(plugin == nil){
        printf("Could not find Java plugin!\n");
        return;
    }
    
    arguments = [NSMutableDictionary dictionaryWithCapacity:10];
    for( it = args.begin(); it != args.end(); ++it ){
        [arguments setObject:QSTRING_TO_NSSTRING(it.data()) forKey:QSTRING_TO_NSSTRING(it.key())];
    }
    setView(WCIFPluginMake(NSMakeRect(0,0,0,0), plugin, nil, @"application/x-java-applet", arguments, NP_EMBED));
}

WCJavaAppletWidget::~WCJavaAppletWidget()
{

}

