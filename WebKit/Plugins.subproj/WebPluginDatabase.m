/*
        WebPluginDatabase.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebAssertions.h>
#import <WebKit/WebBasePluginPackage.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNetscapePluginRepresentation.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginPackage.h>

#import <CoreGraphics/CPSProcesses.h>

#import <Foundation/NSString_NSURLExtras.h>
#import <Foundation/NSURLFileTypeMappings.h>

#define QuickTimePluginIdentifier       @"com.apple.QuickTime Plugin.plugin"
#define JavaCocoaPluginIdentifier 	@"com.apple.JavaPluginCocoa"
#define JavaCarbonPluginIdentifier 	@"com.apple.JavaAppletPlugin"
#define JavaCFMPluginFilename		@"Java Applet Plugin Enabler"
#define JavaCarbonPluginBadVersion 	@"1.0.0"



@implementation WebPluginDatabase

static WebPluginDatabase *database = nil;

+ (WebPluginDatabase *)installedPlugins 
{
    if (!database) {
        database = [[WebPluginDatabase alloc] init];
    }
    return database;
}

static BOOL sIsCocoaIsValid = FALSE;
static BOOL sIsCocoa = FALSE;

- (void)initIsCocoa
{
    const CPSProcessSerNum thisProcess = { 0, kCPSCurrentProcess };
    
    CPSProcessInfoRec *info = (CPSProcessInfoRec *) malloc(sizeof(CPSProcessInfoRec));
    CPSGetProcessInfo((const CPSProcessSerNum*)&thisProcess, info, NULL, 0, NULL, NULL, 0);
    
    sIsCocoa = (info != NULL && info->Flavour == kCPSCocoaApp);
    sIsCocoaIsValid = TRUE;
    
    free(info);
}

- (BOOL)isCocoa
{
    if (!sIsCocoaIsValid) {
        [self initIsCocoa];
    }
    return sIsCocoa;
}

- (BOOL)canUsePlugin:(WebBasePluginPackage *)plugin
{
    // Current versions of the Java plug-ins will not work in a Carbon environment (3283210).
    
    if (!plugin) {
        return NO;
    }
    
    if ([self isCocoa]) {
        return YES;
    }
    
    NSBundle *bundle = [plugin bundle];
    if (bundle) {
        NSString *bundleIdentifier = [bundle bundleIdentifier];
        if ([bundleIdentifier isEqualToString:JavaCocoaPluginIdentifier]) {
            // The Cocoa version of the Java plug-in will never work.
            return NO;
        } else if ([bundleIdentifier isEqualToString:JavaCarbonPluginIdentifier] &&
                   [[[bundle infoDictionary] objectForKey:@"CFBundleVersion"] isEqualToString:JavaCarbonPluginBadVersion]) {
            // The current version of the mach-o Java plug-in won't work.
            return NO;
        }
    } else {
        if ([[plugin filename] isEqualToString:JavaCFMPluginFilename]) {
            // Future versions of the CFM plug-in may work, but there is no way to check its version.
            return NO;
        }
    }
    return YES;
}

- (WebBasePluginPackage *)pluginForKey:(NSString *)key withEnumeratorSelector:(SEL)enumeratorSelector
{
    WebBasePluginPackage *plugin, *CFMPlugin=nil, *machoPlugin=nil, *webPlugin=nil, *QTPlugin=nil;
    NSEnumerator *pluginEnumerator = [plugins objectEnumerator];
    key = [key lowercaseString];

    while ((plugin = [pluginEnumerator nextObject]) != nil) {
        if ([[[plugin performSelector:enumeratorSelector] allObjects] containsObject:key]) {
            if ([[[plugin bundle] bundleIdentifier] _web_isCaseInsensitiveEqualToString:QuickTimePluginIdentifier]) {
                QTPlugin = plugin;
            } else if ([plugin isKindOfClass:[WebPluginPackage class]]) {
                if (webPlugin == nil) {
                    webPlugin = plugin;
                }
            } else if([plugin isKindOfClass:[WebNetscapePluginPackage class]]) {
                WebExecutableType executableType = [(WebNetscapePluginPackage *)plugin executableType];
                if (executableType == WebCFMExecutableType) {
                    if (CFMPlugin == nil) {
                        CFMPlugin = plugin;
                    }
                } else if(executableType == WebMachOExecutableType) {
                    if (machoPlugin == nil) {
                        machoPlugin = plugin;
                    }
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
    if ([self canUsePlugin:webPlugin]) {
        return webPlugin;
    } else if (machoPlugin) {
        return machoPlugin;
    } else if (CFMPlugin) {
        return CFMPlugin;
    } else if (QTPlugin) {
        return QTPlugin;
    } else {
        return nil;
    }
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
        NSString *MIMEType = [[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
        if ([MIMEType length] > 0) {
            plugin = [self pluginForMIMEType:MIMEType];
        }
    }
    return plugin;
}

- (NSArray *)plugins
{
    return [plugins allObjects];
}

static NSArray *pluginLocations(void)
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

- init
{
    self = [super init];
    if (self == nil) {
        return nil;
    }
    [self refresh];
    return self;
    
}
    
- (void)refresh
{
    NSEnumerator *directoryEnumerator = [pluginLocations() objectEnumerator];
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
        NSMutableSet *pluginsToUnload = [plugins mutableCopy];
        [pluginsToUnload minusSet:newPlugins];
#if !LOG_DISABLED
        NSMutableSet *reallyNewPlugins = [newPlugins mutableCopy];
        [reallyNewPlugins minusSet:plugins];
        if ([reallyNewPlugins count] > 0) {
            LOG(Plugins, "New plugins:\n%@", reallyNewPlugins);
        }
        if ([pluginsToUnload count] > 0) {
            LOG(Plugins, "Removed plugins:\n%@", pluginsToUnload);
        }
        [reallyNewPlugins release];
#endif   
        [plugins minusSet:pluginsToUnload];
        [plugins unionSet:newPlugins];   
        [pluginsToUnload release];
        [newPlugins release];
    } else {
        LOG(Plugins, "Plugin database initialization:\n%@", newPlugins);
        plugins = newPlugins;
    }
    
    // Unregister netscape plug-in WebDocumentViews and WebDocumentRepresentations.
    NSMutableDictionary *MIMEToViewClass = [WebFrameView _viewTypesAllowImageTypeOmission:NO];
    NSEnumerator *keyEnumerator = [MIMEToViewClass keyEnumerator];
    NSString *MIMEType;
    while ((MIMEType = [keyEnumerator nextObject]) != nil) {
        if ([MIMEToViewClass objectForKey:MIMEType] == [WebNetscapePluginDocumentView class]) {
            [WebView _unregisterViewClassAndRepresentationClassForMIMEType:MIMEType];
        }
    }
    
    // Register netscape plug-in WebDocumentViews and WebDocumentRepresentations
    NSEnumerator *pluginEnumerator = [plugins objectEnumerator];
    WebBasePluginPackage *plugin;
    while ((plugin = [pluginEnumerator nextObject]) != nil) {
        if ([plugin isKindOfClass:[WebNetscapePluginPackage class]]) {
            BOOL isQuickTime = [[[plugin bundle] bundleIdentifier] _web_isCaseInsensitiveEqualToString:QuickTimePluginIdentifier];
            NSEnumerator *MIMEEnumerator = [plugin MIMETypeEnumerator];
            while ((MIMEType = [MIMEEnumerator nextObject]) != nil) {
                if ([WebView canShowMIMETypeAsHTML:MIMEType] || (isQuickTime && [MIMEToViewClass objectForKey:MIMEType] != nil)) {
                    // Don't allow plug-ins to override our core HTML types and don't allow the QT plug-in to override any types 
                    // because it claims many that we can handle ourselves.
                    continue;
                }
                [WebView registerViewClass:[WebNetscapePluginDocumentView class] representationClass:[WebNetscapePluginRepresentation class] forMIMEType:MIMEType];
            }
        } else {
            if (![plugin isLoaded]) {
                if (!pendingPluginLoads) {
                    pendingPluginLoads = [[NSMutableSet alloc] init];
                }
                [pendingPluginLoads addObject:plugin];
            }
        }
    }
}

- (void)loadPluginIfNeededForMIMEType: (NSString *)MIMEType
{
    NSEnumerator *pluginEnumerator = [pendingPluginLoads objectEnumerator];
    WebBasePluginPackage *plugin;
    while ((plugin = [pluginEnumerator nextObject]) != nil) {
        if ([[[plugin MIMETypeEnumerator] allObjects] containsObject:MIMEType]) {
            [plugin load];
            [pendingPluginLoads removeObject:plugin];
        }
    }
}

- (void)dealloc
{
    [plugins release];
    [pendingPluginLoads release];
    [super dealloc];
}

@end
