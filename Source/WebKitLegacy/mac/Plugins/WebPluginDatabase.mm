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

#import "WebPluginDatabase.h"

#import "WebBasePluginPackage.h"
#import "WebDataSourcePrivate.h"
#import "WebFrame.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentation.h"
#import "WebHTMLViewInternal.h"
#import "WebKitLogging.h"
#import "WebNSFileManagerExtras.h"
#import "WebPluginController.h"
#import "WebPluginPackage.h"
#import "WebViewInternal.h"
#import "WebViewPrivate.h"
#import <pal/spi/cocoa/NSURLFileTypeMappingsSPI.h>
#import <wtf/Assertions.h>

#if PLATFORM(IOS_FAMILY)
#import "WebUIKitSupport.h"
#endif

static void checkCandidate(WebBasePluginPackage **currentPlugin, WebBasePluginPackage **candidatePlugin);

@interface WebPluginDatabase (Internal)
+ (NSArray *)_defaultPlugInPaths;
- (NSArray *)_plugInPaths;
- (void)_addPlugin:(WebBasePluginPackage *)plugin;
- (void)_removePlugin:(WebBasePluginPackage *)plugin;
- (NSMutableSet *)_scanForNewPlugins;
@end

@implementation WebPluginDatabase

static RetainPtr<WebPluginDatabase>& sharedDatabase()
{
    static NeverDestroyed<RetainPtr<WebPluginDatabase>> sharedDatabase;
    return sharedDatabase;
}

+ (WebPluginDatabase *)sharedDatabase 
{
    auto& database = sharedDatabase();
    if (!database) {
        database = adoptNS([[WebPluginDatabase alloc] init]);
        [database setPlugInPaths:[self _defaultPlugInPaths]];
        [database refresh];
    }
    
    return database.get();
}

+ (WebPluginDatabase *)sharedDatabaseIfExists
{
    return sharedDatabase().get();
}

+ (void)closeSharedDatabase 
{
    [sharedDatabase() close];
}

static void checkCandidate(WebBasePluginPackage * __strong *currentPlugin, WebBasePluginPackage * __strong *candidatePlugin)
{
    if (!*currentPlugin) {
        *currentPlugin = *candidatePlugin;
        return;
    }

    if ([*currentPlugin bundleIdentifier] == [*candidatePlugin bundleIdentifier] && [*candidatePlugin versionNumber] > [*currentPlugin versionNumber]) 
        *currentPlugin = *candidatePlugin;
}

struct PluginPackageCandidates {
    PluginPackageCandidates()
        : webPlugin(nil)
        , netscapePlugin(nil)
    {
    }
    
    void update(WebBasePluginPackage *plugin)
    {
        if ([plugin isKindOfClass:[WebPluginPackage class]]) {
            checkCandidate(&webPlugin, &plugin);
            return;
        }
        ASSERT_NOT_REACHED();
    }
    
    WebBasePluginPackage *bestCandidate()
    {
        // Allow other plug-ins to win over QT because if the user has installed a plug-in that can handle a type
        // that the QT plug-in can handle, they probably intended to override QT.
        if (webPlugin && ![webPlugin isQuickTimePlugIn])
            return webPlugin;
    
        if (netscapePlugin && ![netscapePlugin isQuickTimePlugIn])
            return netscapePlugin;
        
        if (webPlugin)
            return webPlugin;
        if (netscapePlugin)
            return netscapePlugin;

        return nil;
    }
    
    WebBasePluginPackage *webPlugin;
    WebBasePluginPackage *netscapePlugin;
};

- (WebBasePluginPackage *)pluginForMIMEType:(NSString *)MIMEType
{
    PluginPackageCandidates candidates;
    
    MIMEType = [MIMEType lowercaseString];
    NSEnumerator *pluginEnumerator = [plugins objectEnumerator];
    
    while (WebBasePluginPackage *plugin = [pluginEnumerator nextObject]) {
        if ([plugin supportsMIMEType:MIMEType])
            candidates.update(plugin);
    }
    
    return candidates.bestCandidate();
}

- (WebBasePluginPackage *)pluginForExtension:(NSString *)extension
{
    PluginPackageCandidates candidates;
    
    extension = [extension lowercaseString];
    NSEnumerator *pluginEnumerator = [plugins objectEnumerator];
    
    while (WebBasePluginPackage *plugin = [pluginEnumerator nextObject]) {
        if ([plugin supportsExtension:extension])
            candidates.update(plugin);
    }
    
    WebBasePluginPackage *plugin = candidates.bestCandidate();
    
    if (!plugin) {
        // If no plug-in was found from the extension, attempt to map from the extension to a MIME type
        // and find the a plug-in from the MIME type. This is done in case the plug-in has not fully specified
        // an extension <-> MIME type mapping.
        NSString *MIMEType = [[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
        if ([MIMEType length] > 0)
            plugin = [self pluginForMIMEType:MIMEType];
    }
    return plugin;
}

- (NSArray *)plugins
{
    return [plugins allValues];
}

static RetainPtr<NSArray>& additionalWebPlugInPaths()
{
    static NeverDestroyed<RetainPtr<NSArray>> _additionalWebPlugInPaths;
    return _additionalWebPlugInPaths;
}

+ (void)setAdditionalWebPlugInPaths:(NSArray *)additionalPaths
{
    if (additionalPaths == additionalWebPlugInPaths())
        return;
    
    additionalWebPlugInPaths() = adoptNS([additionalPaths copy]);

    // One might be tempted to add additionalWebPlugInPaths to the global WebPluginDatabase here.
    // For backward compatibility with earlier versions of the +setAdditionalWebPlugInPaths: SPI,
    // we need to save a copy of the additional paths and not cause a refresh of the plugin DB
    // at this time.
    // See Radars 4608487 and 4609047.
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
    NSEnumerator *pluginEnumerator = [[self plugins] objectEnumerator];
    WebBasePluginPackage *plugin;
    while ((plugin = [pluginEnumerator nextObject]) != nil)
        [self _removePlugin:plugin];
    [plugins release];
    plugins = nil;
}

- (id)init
{
    if (!(self = [super init]))
        return nil;
        
    registeredMIMETypes = [[NSMutableSet alloc] init];
    pluginInstanceViews = [[NSMutableSet alloc] init];
    
    return self;
}

- (void)dealloc
{
    [plugInPaths release];
    [plugins release];
    [registeredMIMETypes release];
    [pluginInstanceViews release];
    
    [super dealloc];
}

- (void)refresh
{
    // This method does a bit of autoreleasing, so create an autorelease pool to ensure that calling
    // -refresh multiple times does not bloat the default pool.
    @autoreleasepool {
        // Create map from plug-in path to WebBasePluginPackage
        if (!plugins)
            plugins = [[NSMutableDictionary alloc] initWithCapacity:12];

        // Find all plug-ins on disk
        NSMutableSet *newPlugins = [self _scanForNewPlugins];

        // Find plug-ins to remove from database (i.e., plug-ins that no longer exist on disk)
        NSMutableSet *pluginsToRemove = [NSMutableSet set];
        NSEnumerator *pluginEnumerator = [plugins objectEnumerator];
        WebBasePluginPackage *plugin;
        while ((plugin = [pluginEnumerator nextObject]) != nil) {
            // Any plug-ins that were removed from disk since the last refresh should be removed from
            // the database.
            if (![newPlugins containsObject:plugin])
                [pluginsToRemove addObject:plugin];

            // Remove every member of 'plugins' from 'newPlugins'. After this loop exits, 'newPlugins'
            // will be the set of new plug-ins that should be added to the database.
            [newPlugins removeObject:plugin];
        }

#if !LOG_DISABLED
        if ([newPlugins count] > 0)
            LOG(Plugins, "New plugins:\n%@", newPlugins);
        if ([pluginsToRemove count] > 0)
            LOG(Plugins, "Removed plugins:\n%@", pluginsToRemove);
#endif

        // Remove plugins from database
        pluginEnumerator = [pluginsToRemove objectEnumerator];
        while ((plugin = [pluginEnumerator nextObject]) != nil)
            [self _removePlugin:plugin];

        // Add new plugins to database
        pluginEnumerator = [newPlugins objectEnumerator];
        while ((plugin = [pluginEnumerator nextObject]) != nil)
            [self _addPlugin:plugin];

        // Build a list of MIME types.
        auto MIMETypes = adoptNS([[NSMutableSet alloc] init]);
        pluginEnumerator = [plugins objectEnumerator];
        while ((plugin = [pluginEnumerator nextObject])) {
            const auto& pluginInfo = [plugin pluginInfo];
            for (size_t i = 0; i < pluginInfo.mimes.size(); ++i)
                [MIMETypes addObject:pluginInfo.mimes[i].type];
        }

        // Register plug-in views and representations.
        NSEnumerator *MIMEEnumerator = [MIMETypes objectEnumerator];
        NSString *MIMEType;
        while ((MIMEType = [MIMEEnumerator nextObject]) != nil) {
            [registeredMIMETypes addObject:MIMEType];

            if ([WebView canShowMIMETypeAsHTML:MIMEType]) {
                // Don't allow plug-ins to override our core HTML types.
                continue;
            }
            plugin = [self pluginForMIMEType:MIMEType];
            if ([plugin isJavaPlugIn]) {
                // Don't register the Java plug-in for a document view since Java files should be downloaded when not embedded.
                continue;
            }
            if ([plugin isQuickTimePlugIn] && [[WebFrameView _viewTypesAllowImageTypeOmission:NO] objectForKey:MIMEType]) {
                // Don't allow the QT plug-in to override any types because it claims many that we can handle ourselves.
                continue;
            }

            if (self == sharedDatabase())
                [WebView _registerPluginMIMEType:MIMEType];
        }
    }
}

- (BOOL)isMIMETypeRegistered:(NSString *)MIMEType
{
    return [registeredMIMETypes containsObject:MIMEType];
}

- (void)addPluginInstanceView:(NSView *)view
{
    [pluginInstanceViews addObject:view];
}

- (void)removePluginInstanceView:(NSView *)view
{
    [pluginInstanceViews removeObject:view];
}

- (void)removePluginInstanceViewsFor:(WebFrame*)webFrame
{
    // This handles handles the case where a frame or view is being destroyed and the plugin needs to be removed from the list first
    
    if( [pluginInstanceViews count] == 0 )
        return;

    NSView <WebDocumentView> *documentView = [[webFrame frameView] documentView]; 
    if ([documentView isKindOfClass:[WebHTMLView class]]) {
        for (NSView *subview in [documentView subviews]) {
            if ([WebPluginController isPlugInView:subview])
                [pluginInstanceViews removeObject:subview];
        }
    }
}

- (void)destroyAllPluginInstanceViews
{
    NSView *view;
    NSArray *pli = [pluginInstanceViews allObjects];
    NSEnumerator *enumerator = [pli objectEnumerator];
    while ((view = [enumerator nextObject]) != nil) {
        if ([WebPluginController isPlugInView:view]) {
            ASSERT([[view superview] isKindOfClass:[WebHTMLView class]]);
            ASSERT([[view superview] respondsToSelector:@selector(_destroyAllWebPlugins)]);
            // this will actually destroy all plugin instances for a webHTMLView and remove them from this list
            [[view superview] performSelector:@selector(_destroyAllWebPlugins)]; 
        }
    }
}
    
@end

@implementation WebPluginDatabase (Internal)

+ (NSArray *)_defaultPlugInPaths
{
#if PLATFORM(IOS_FAMILY)
    return @[];
#else
    // Plug-ins are found in order of precedence.
    // If there are duplicates, the first found plug-in is used.
    // For example, if there is a QuickTime.plugin in the users's home directory
    // that is used instead of the /Library/Internet Plug-ins version.
    // The purpose is to allow non-admin users to update their plug-ins.
    return @[
        [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Internet Plug-Ins"],
        @"/Library/Internet Plug-Ins",
        [[NSBundle mainBundle] builtInPlugInsPath],
    ];
#endif
}

- (NSArray *)_plugInPaths
{
    if (self == sharedDatabase() && additionalWebPlugInPaths()) {
        // Add additionalWebPlugInPaths to the global WebPluginDatabase.  We do this here for
        // backward compatibility with earlier versions of the +setAdditionalWebPlugInPaths: SPI,
        // which simply saved a copy of the additional paths and did not cause the plugin DB to 
        // refresh.  See Radars 4608487 and 4609047.
        auto modifiedPlugInPaths = adoptNS([plugInPaths mutableCopy]);
        [modifiedPlugInPaths addObjectsFromArray:additionalWebPlugInPaths().get()];
        return modifiedPlugInPaths.autorelease();
    }
    return plugInPaths;
}

- (void)_addPlugin:(WebBasePluginPackage *)plugin
{
    ASSERT(plugin);
    NSString *pluginPath = [plugin path];
    ASSERT(pluginPath);
    [plugins setObject:plugin forKey:pluginPath];
    [plugin wasAddedToPluginDatabase:self];
}

- (void)_removePlugin:(WebBasePluginPackage *)plugin
{    
    ASSERT(plugin);

    // Unregister plug-in's MIME type registrations
    const auto& pluginInfo = [plugin pluginInfo];
    for (size_t i = 0; i < pluginInfo.mimes.size(); ++i) {
        NSString *MIMEType = pluginInfo.mimes[i].type;

        if ([registeredMIMETypes containsObject:MIMEType]) {
            if (self == sharedDatabase())
                [WebView _unregisterPluginMIMEType:MIMEType];
            [registeredMIMETypes removeObject:MIMEType];
        }
    }

    // Remove plug-in from database
    NSString *pluginPath = [plugin path];
    ASSERT(pluginPath);
    auto protectedPlugin = retainPtr(plugin);
    [plugins removeObjectForKey:pluginPath];
    [plugin wasRemovedFromPluginDatabase:self];
}

- (NSMutableSet *)_scanForNewPlugins
{
    NSMutableSet *newPlugins = [NSMutableSet set];
    NSEnumerator *directoryEnumerator = [[self _plugInPaths] objectEnumerator];
    auto uniqueFilenames = adoptNS([[NSMutableSet alloc] init]);
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *pluginDirectory;
    while ((pluginDirectory = [directoryEnumerator nextObject]) != nil) {
        // Get contents of each plug-in directory
        NSEnumerator *filenameEnumerator = [[fileManager contentsOfDirectoryAtPath:pluginDirectory error:NULL] objectEnumerator];
        NSString *filename;
        while ((filename = [filenameEnumerator nextObject]) != nil) {
            // Unique plug-ins by filename
            if ([uniqueFilenames containsObject:filename])
                continue;
            [uniqueFilenames addObject:filename];
            
            // Create a plug-in package for this path
            NSString *pluginPath = [pluginDirectory stringByAppendingPathComponent:filename];
            WebBasePluginPackage *pluginPackage = [plugins objectForKey:pluginPath];
            if (!pluginPackage)
                pluginPackage = [WebBasePluginPackage pluginWithPath:pluginPath];
            if (pluginPackage)
                [newPlugins addObject:pluginPackage];
        }
    }
    
    return newPlugins;
}

@end
