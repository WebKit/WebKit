/*
 * Copyright (C) 2005 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebCore/PluginData.h>
#import <wtf/RetainPtr.h>

typedef void (*BP_CreatePluginMIMETypesPreferencesFuncPtr)(void);

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WAKAppKitStubs.h>
#endif

@class WebPluginDatabase;

@protocol WebPluginManualLoader
- (void)pluginView:(NSView *)pluginView receivedResponse:(NSURLResponse *)response;
- (void)pluginView:(NSView *)pluginView receivedData:(NSData *)data;
- (void)pluginView:(NSView *)pluginView receivedError:(NSError *)error;
- (void)pluginViewFinishedLoading:(NSView *)pluginView;
@end

#define WebPluginExtensionsKey          @"WebPluginExtensions"
#define WebPluginDescriptionKey         @"WebPluginDescription"
#define WebPluginLocalizationNameKey    @"WebPluginLocalizationName"
#define WebPluginMIMETypesFilenameKey   @"WebPluginMIMETypesFilename"
#define WebPluginMIMETypesKey           @"WebPluginMIMETypes"
#define WebPluginNameKey                @"WebPluginName"
#define WebPluginTypeDescriptionKey     @"WebPluginTypeDescription"
#define WebPluginTypeEnabledKey         @"WebPluginTypeEnabled"

@interface WebBasePluginPackage : NSObject
{
    NSMutableSet *pluginDatabases;
    
    WTF::String path;
    WebCore::PluginInfo pluginInfo;

    RetainPtr<CFBundleRef> cfBundle;

    BP_CreatePluginMIMETypesPreferencesFuncPtr BP_CreatePluginMIMETypesPreferences;
}

+ (WebBasePluginPackage *)pluginWithPath:(NSString *)pluginPath;
- (id)initWithPath:(NSString *)pluginPath;

- (BOOL)getPluginInfoFromPLists;

- (BOOL)load;
- (void)unload;

- (const WTF::String&)path;

- (const WebCore::PluginInfo&)pluginInfo;

- (String)bundleIdentifier;
- (String)bundleVersion;

- (BOOL)supportsExtension:(const WTF::String&)extension;
- (BOOL)supportsMIMEType:(const WTF::String&)MIMEType;

- (NSString *)MIMETypeForExtension:(const WTF::String&)extension;

- (BOOL)isQuickTimePlugIn;
- (BOOL)isJavaPlugIn;

- (BOOL)isNativeLibraryData:(NSData *)data;
- (UInt32)versionNumber;
- (void)wasAddedToPluginDatabase:(WebPluginDatabase *)database;
- (void)wasRemovedFromPluginDatabase:(WebPluginDatabase *)database;

@end
