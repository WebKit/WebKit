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

#include <KWQPlugin.h>
#include <KWQView.h>
#include <kwqdebug.h>

KWQPlugin::KWQPlugin(QWidget *parent, WKPlugin *plugin, const QString &url, const QString &serviceType, const QStringList &args)
{
    NSMutableDictionary *arguments;
    NSString *arg;
    NSRange r1, r2, r3;
    uint i;
    
    arguments = [NSMutableDictionary dictionaryWithCapacity:10];
    for(i=0; i<args.count(); i++){
    arg = QSTRING_TO_NSSTRING(args[i]);
        r1 = [arg rangeOfString:@"="]; // parse out attributes and values
        r2 = [arg rangeOfString:@"\""];
        r3.location = r2.location + 1;
        r3.length = [arg length] - r2.location - 2; // don't include quotes
        [arguments setObject:[arg substringWithRange:r3] forKey:[arg substringToIndex:r1.location]];
    }
    
    setView([[[WKPluginView alloc] initWithFrame: NSMakeRect (0,0,0,0) widget: this plugin: plugin url:QSTRING_TO_NSSTRING(url) mime:QSTRING_TO_NSSTRING(serviceType) arguments:arguments] autorelease]);
}

KWQPlugin::~KWQPlugin()
{

}

