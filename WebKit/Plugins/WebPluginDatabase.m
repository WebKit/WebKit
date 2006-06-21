/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import <WebKit/WebPluginDatabase.h>

#import <JavaScriptCore/Assertions.h>
#import <WebKit/WebBasePluginPackage.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameViewInternal.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPluginPackage.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKitSystemInterface.h>

@interface WebPluginDatabase (Internal)
+ (NSArray *)_defaultPlugInPaths;
@end

@implementation WebPluginDatabase

static WebPluginDatabase *database = nil;

+ (WebPluginDatabase *)installedPlugins 
{
    if (!database) {
        database = [[WebPluginDatabase alloc] init];
        [database setPlugInPaths:[self _defaultPlugInPaths]];
        [database refresh];
    }
    
    return database;
}

- (WebBasePluginPackage *)pluginForKey:(NSString *)key withEnumeratorSelector:(SEL)enumeratorSelector
{
    WebBasePluginPackage *plugin, *CFMPlugin=nil, *machoPlugin=nil, *webPlugin=nil;
    NSEnumerator *pluginEnumerator = [plugins objectEnumerator];
    key = [key lowercaseString];

    while ((plugin = [pluginEnumerator nextObject]) != nil) {
        if ([[[plugin performSelector:enumeratorSelector] allObjects] containsObject:key]) {
            if ([plugin isKindOfClass:[WebPluginPackage class]]) {
                if (!webPlugin)
                    webPlugin = plugin;
            } else if([plugin isKindOfClass:[WebNetscapePluginPackage class]]) {
                WebExecutableType executableType = [(WebNetscapePluginPackage *)plugin executableType];
                if (executableType == WebCFMExecutableType) {
                    if (!CFMPlugin)
                        CFMPlugin = plugin;
                } else if (executableType == WebMachOExecutableType) {
                    if (!machoPlugin)
                        machoPlugin = plugin;
                } else {
                    ASSERT_NOT_REACHED();
                }
            } else {
                ASSERT_NOT_REACHED();
            }
        }
    }

    // Allow other plug-ins to win over QT because if the user has installed a plug-in that can handle a type
    // that the QT plug-in can handle, they probably intended to override QT.
    if (webPlugin && ![webPlugin isQuickTimePlugIn])
        return webPlugin;
    else if (machoPlugin && ![machoPlugin isQuickTimePlugIn])
        return machoPlugin;
    else if (CFMPlugin && ![CFMPlugin isQuickTimePlugIn])
        return CFMPlugin;
    else if (webPlugin)
        return webPlugin;
    else if (machoPlugin)
        return machoPlugin;
    else if (CFMPlugin)
        return CFMPlugin;
    return nil;
}

- (WebBasePluginPackage *)pluginForMIMEType:(NSString *)MIMEType
{
    return [self pluginForKey:[MIMEType lowercaseString]
       withEnumeratorSelector:@selector(MIMETypeEnumerator)];
}

- (WebBasePluginPackage *)pluginForExtension:(NSString *)extension
{
    WebBasePluginPackage *plugin = [self pluginForKey:[extension lowercaseString]
                               withEnumeratorSelector:@selector(extensionEnumerator)];
    if (!plugin) {
        // If no plug-in was found from the extension, attempt to map from the extension to a MIME type
        // and find the a plug-in from the MIME type. This is done in case the plug-in has not fully specified
        // an extension <-> MIME type mapping.
        NSString *MIMEType = WKGetMIMETypeForExtension(extension);
        if ([MIMEType length] > 0)
            plugin = [self pluginForMIMEType:MIMEType];
    }
    return plugin;
}

- (NSArray *)plugins
{
    return [plugins allObjects];
}

+ (void)setAdditionalWebPlugInPaths:(NSArray *)additionalPaths
{
    NSMutableArray *newPlugInPaths = [[self _defaultPlugInPaths] mutableCopy];
    [newPlugInPaths addObjectsFromArray:additionalPaths];
    [[WebPluginDatabase installedPlugins] setPlugInPaths:newPlugInPaths];
    [newPlugInPaths release];
}

- (void)setPlugInPaths:(NSArray *)newPaths
{
    if (plugInPaths == newPaths)
        return;
        
    [plugInPaths release];
    plugInPaths = [newPaths copy];
}

- (void)close
{
    [plugins makeObjectsPerformSelector:@selector(wasRemovedFromPluginDatabase:) withObject:self];
    [plugins release];
    plugins = nil;
}

- (id)init
{
    if (!(self = [super init]))
        return nil;
        
    registeredMIMETypes = [[NSMutableSet alloc] init];
    
    return self;
}

- (void)dealloc
{
    [plugInPaths release];
    [plugins release];
    [registeredMIMETypes release];
    
    [super dealloc];
}

- (void)refresh
{
    NSEnumerator *directoryEnumerator = [plugInPaths objectEnumerator];
    NSMutableSet *uniqueFilenames = [[NSMutableArray alloc] init];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSMutableSet *newPlugins = [[NSMutableSet alloc] init];
    NSString *pluginDirectory;
    
    // Create a new set of plug-ins.
    while ((pluginDirectory = [directoryEnumerator nextObject]) != nil) {
        NSEnumerator *filenameEnumerator = [[fileManager directoryContentsAtPath:pluginDirectory] objectEnumerator];
        NSString *filename;
        while ((filename = [filenameEnumerator nextObject]) != nil) {
            if (![uniqueFilenames containsObject:filename]) {
                [uniqueFilenames addObject:filename];
                NSString *pluginPath = [pluginDirectory stringByAppendingPathComponent:filename];
                WebBasePluginPackage *pluginPackage = [WebBasePluginPackage pluginWithPath:pluginPath];
                if (pluginPackage) {
                    [newPlugins addObject:pluginPackage];
                }
            }
        }
    }
    
    [uniqueFilenames release];
    
    //  Remove all uninstalled plug-ins and add the new plug-ins.
    if (plugins) {
        // Unregister plug-in views and representations
        NSEnumerator *pluginEnumerator = [plugins objectEnumerator];
        WebBasePluginPackage *pluginPackage;
        while ((pluginPackage = [pluginEnumerator nextObject])) {
            NSEnumerator *MIMETypeEnumerator = [pluginPackage MIMETypeEnumerator];
            NSString *MIMEType;
            while ((MIMEType = [MIMETypeEnumerator nextObject])) {
                if ([registeredMIMETypes containsObject:MIMEType]) {
                    if (self == database)
                        [WebView _unregisterViewClassAndRepresentationClassForMIMEType:MIMEType];
                    [registeredMIMETypes removeObject:MIMEType];
                }
            }
        }
        
        NSMutableSet *pluginsToUnload = [plugins mutableCopy];
        [pluginsToUnload minusSet:newPlugins];
        [newPlugins minusSet:plugins];
#if !LOG_DISABLED
        if ([newPlugins count] > 0) {
            LOG(Plugins, "New plugins:\n%@", newPlugins);
        }
        if ([pluginsToUnload count] > 0) {
            LOG(Plugins, "Removed plugins:\n%@", pluginsToUnload);
        }
#endif
        // Unload plugins
        [pluginsToUnload makeObjectsPerformSelector:@selector(wasRemovedFromPluginDatabase:) withObject:self];
        [plugins minusSet:pluginsToUnload];
        [pluginsToUnload release];

        // Add new plugins
        [plugins unionSet:newPlugins];
        [newPlugins makeObjectsPerformSelector:@selector(wasAddedToPluginDatabase:) withObject:self];
        [newPlugins release];
    } else {
        LOG(Plugins, "Plugin database initialization:\n%@", newPlugins);
        plugins = newPlugins;
        [newPlugins makeObjectsPerformSelector:@selector(wasAddedToPluginDatabase:) withObject:self];
    }
    
    // Build a list of MIME types.
    NSMutableSet *MIMETypes = [[NSMutableSet alloc] init];
    NSEnumerator *pluginEnumerator = [plugins objectEnumerator];
    WebBasePluginPackage *plugin;
    while ((plugin = [pluginEnumerator nextObject]) != nil) {
        [MIMETypes addObjectsFromArray:[[plugin MIMETypeEnumerator] allObjects]];
    }
    
    // Register plug-in views and representations.
    NSEnumerator *MIMEEnumerator = [[MIMETypes allObjects] objectEnumerator];
    [MIMETypes release];
    NSString *MIMEType;
    while ((MIMEType = [MIMEEnumerator nextObject]) != nil) {
        if ([WebView canShowMIMETypeAsHTML:MIMEType])
            // Don't allow plug-ins to override our core HTML types.
            continue;
        plugin = [self pluginForMIMEType:MIMEType];
        if ([plugin isJavaPlugIn])
            // Don't register the Java plug-in for a document view since Java files should be downloaded when not embedded.
            continue;
        if ([plugin isQuickTimePlugIn] && [[WebFrameView _viewTypesAllowImageTypeOmission:NO] objectForKey:MIMEType])
            // Don't allow the QT plug-in to override any types because it claims many that we can handle ourselves.
            continue;
        
        if (self == database)
            [WebView registerViewClass:[WebHTMLView class] representationClass:[WebHTMLRepresentation class] forMIMEType:MIMEType];
        [registeredMIMETypes addObject:MIMEType];
    }
}

- (BOOL)isMIMETypeRegistered:(NSString *)MIMEType
{
    return [registeredMIMETypes containsObject:MIMEType];
}

@end

@implementation WebPluginDatabase (Internal)

+ (NSArray *)_defaultPlugInPaths
{
    // Plug-ins are found in order of precedence.
    // If there are duplicates, the first found plug-in is used.
    // For example, if there is a QuickTime.plugin in the users's home directory
    // that is used instead of the /Library/Internet Plug-ins version.
    // The purpose is to allow non-admin users to update their plug-ins.
    return [NSArray arrayWithObjects:
        [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Internet Plug-Ins"],
        @"/Library/Internet Plug-Ins",
        [[NSBundle mainBundle] builtInPlugInsPath],
        nil];
}

@end
