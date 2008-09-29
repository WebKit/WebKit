/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#import "config.h"
#import "PluginData.h"

#import "BlockExceptions.h"
#import "Logging.h"
#import "WebCoreViewFactory.h"

namespace WebCore {

void PluginData::initPlugins()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSArray* plugins = [[WebCoreViewFactory sharedFactory] pluginsInfo];
    for (unsigned int i = 0; i < [plugins count]; ++i) {
        PluginInfo* pluginInfo = new PluginInfo;

        id <WebCorePluginInfo> plugin = [plugins objectAtIndex:i];
    
        pluginInfo->name = [plugin name];
        pluginInfo->file = [plugin filename];
        pluginInfo->desc = [plugin pluginDescription];

        NSEnumerator* MIMETypeEnumerator = [plugin MIMETypeEnumerator];
        while (NSString* MIME = [MIMETypeEnumerator nextObject]) {
            MimeClassInfo* mime = new MimeClassInfo;
            pluginInfo->mimes.append(mime);
            mime->type = String(MIME).lower();
            mime->suffixes = [[plugin extensionsForMIMEType:MIME] componentsJoinedByString:@","];
            mime->desc = [plugin descriptionForMIMEType:MIME];
            mime->plugin = pluginInfo;
        }

        m_plugins.append(pluginInfo);
    }
    
    END_BLOCK_OBJC_EXCEPTIONS;

    return;
}

void PluginData::refresh()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [[WebCoreViewFactory sharedFactory] refreshPlugins];
    END_BLOCK_OBJC_EXCEPTIONS;
}

}

