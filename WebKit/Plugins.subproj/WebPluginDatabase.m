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

#define JavaCocoaPluginIdentifier 	@"com.apple.JavaPluginCocoa"

#define JavaCarbonPluginIdentifier 	@"com.apple.JavaAppletPlugin"
#define JavaCarbonPluginBadVersion 	@"1.0.0"

#define JavaCFMPluginFilename		@"Java Applet Plugin Enabler"

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
    WebBasePluginPackage *plugin, *CFMPlugin=nil, *machoPlugin=nil, *webPlugin=nil;
    NSEnumerator *pluginEnumerator = [plugins objectEnumerator];
    key = [key lowercaseString];

    while ((plugin = [pluginEnumerator nextObject]) != nil) {
        if ([[[plugin performSelector:enumeratorSelector] allObjects] containsObject:key]) {
            if ([plugin isKindOfClass:[WebPluginPackage class]]) {
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

    if ([self canUsePlugin:webPlugin]) {
        return webPlugin;
    } else if (machoPlugin) {
        return machoPlugin;
    } else if (CFMPlugin) {
        return CFMPlugin;
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
    return [self pluginForKey:[extension lowercaseString]
       withEnumeratorSelector:@selector(extensionEnumerator)];
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
    // but do not override other document views and representations.
    NSEnumerator *pluginEnumerator = [plugins objectEnumerator];
    WebBasePluginPackage *plugin;
    while ((plugin = [pluginEnumerator nextObject]) != nil) {
        if ([plugin isKindOfClass:[WebNetscapePluginPackage class]]) {
            NSEnumerator *MIMEEnumerator = [plugin MIMETypeEnumerator];
            while ((MIMEType = [MIMEEnumerator nextObject]) != nil) {
                if ([MIMEToViewClass objectForKey:MIMEType] == nil) {
                    [WebView registerViewClass:[WebNetscapePluginDocumentView class] representationClass:[WebNetscapePluginRepresentation class] forMIMEType:MIMEType];
                }
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
