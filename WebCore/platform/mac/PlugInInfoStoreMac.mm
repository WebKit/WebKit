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

#import "config.h"
#import "PluginInfoStore.h"

#import "BlockExceptions.h"
#import "Logging.h"
#import "WebCoreViewFactory.h"

namespace WebCore {

String PluginInfoStore::pluginNameForMIMEType(const String& mimeType)
{
    return [[WebCoreViewFactory sharedFactory] pluginNameForMIMEType:mimeType];
}

PluginInfo *PluginInfoStore::createPluginInfoForPluginAtIndex(unsigned index)
{
    PluginInfo *pluginInfo = new PluginInfo;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    id <WebCorePluginInfo> plugin = [[[WebCoreViewFactory sharedFactory] pluginsInfo] objectAtIndex:index];
    
    pluginInfo->name = [plugin name];
    pluginInfo->file = [plugin filename];
    pluginInfo->desc = [plugin pluginDescription];

    NSEnumerator *MIMETypeEnumerator = [plugin MIMETypeEnumerator];
    while (NSString *MIME = [MIMETypeEnumerator nextObject]) {
        MimeClassInfo *mime = new MimeClassInfo;
        pluginInfo->mimes.append(mime);
        mime->type = String(MIME).lower();
        mime->suffixes = [[plugin extensionsForMIMEType:MIME] componentsJoinedByString:@","];
        mime->desc = [plugin descriptionForMIMEType:MIME];
        mime->plugin = pluginInfo;
    }
    
    return pluginInfo;

    END_BLOCK_OBJC_EXCEPTIONS;
    
    if (pluginInfo && !pluginInfo->mimes.isEmpty())
        deleteAllValues(pluginInfo->mimes);
    delete pluginInfo;

    return 0;
}

unsigned PluginInfoStore::pluginCount() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[[WebCoreViewFactory sharedFactory] pluginsInfo] count];
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return 0;
}

bool PluginInfoStore::supportsMIMEType(const String& mimeType)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[WebCoreViewFactory sharedFactory] pluginSupportsMIMEType:mimeType];
    END_BLOCK_OBJC_EXCEPTIONS;    
    
    return NO;
}

void refreshPlugins(bool reloadOpenPages)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [[WebCoreViewFactory sharedFactory] refreshPlugins:reloadOpenPages];
    END_BLOCK_OBJC_EXCEPTIONS;
}

}

