/*
        WebPluginDatabase.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebAssertions.h>
#import <WebKit/WebBasePluginPackage.h>
#import <WebKit/WebDataSource.h>
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

- (WebBasePluginPackage *)pluginForKey:(NSString *)key withEnumeratorSelector:(SEL)enumeratorSelector
{
    WebBasePluginPackage *plugin, *CFMPlugin=nil, *machoPlugin=nil, *webPlugin=nil;
    NSString *lowercaseKey = [key lowercaseString];
    uint i;

    for (i=0; i<[plugins count]; i++) {
        plugin = [plugins objectAtIndex:i];
        if ([[[plugin performSelector:enumeratorSelector] allObjects] containsObject:lowercaseKey]) {
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

    // The Cocoa Java plug-in won't work in a Carbon app.
    if (webPlugin && ![self isCocoa] && [[[webPlugin bundle] bundleIdentifier] isEqualToString:JavaCocoaPluginIdentifier]) {
        webPlugin = nil;
    }
    
    if (webPlugin) {
        return webPlugin;
    } else if (machoPlugin) {
        return machoPlugin;
    } else {
        return CFMPlugin;
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
    return plugins;
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
    
    NSArray *pluginDirectories = pluginLocations();
    
    NSFileManager *fileManager = [NSFileManager defaultManager];

    NSMutableArray *pluginPaths = [NSMutableArray arrayWithCapacity:10];
    NSMutableSet *filenames = [NSMutableSet setWithCapacity:10];

    NSArray *files;
    NSString *file;
    uint i, j, n;
    
    for (i = 0; i < [pluginDirectories count]; i++) {
        files = [fileManager directoryContentsAtPath:[pluginDirectories objectAtIndex:i]];
        for (n = 0; n < [files count]; n++) {
            file = [files objectAtIndex:n];
            if (![filenames containsObject:file]) { // avoid duplicates
                [filenames addObject:file];
                [pluginPaths addObject:[[pluginDirectories objectAtIndex:i] stringByAppendingPathComponent:file]];
            }
        }
    }
    
    NSMutableArray *pluginArray = [NSMutableArray arrayWithCapacity:[pluginPaths count]];
    
    for (i = 0; i < [pluginPaths count]; i++) {
        WebBasePluginPackage *pluginPackage = [WebBasePluginPackage pluginWithPath:[pluginPaths objectAtIndex:i]];
        if (pluginPackage) {
            [pluginArray addObject:pluginPackage];
            LOG(Plugins, "Found plugin: %s", [[pluginPackage name] lossyCString]);
            LOG(Plugins, "%s", [[pluginPackage description] lossyCString]);
        }
    }

    plugins = [pluginArray copy];

    // Register plug-in WebDocumentViews and WebDocumentRepresentations
    NSArray *viewTypes = [[WebFrameView _viewTypesAllowImageTypeOmission:NO] allKeys];
    NSArray *mimes;
    NSString *mime;
    WebBasePluginPackage *plugin;
    uint pluginCount, mimeCount;
    
    pluginCount = [plugins count];
    for (i = 0; i < pluginCount; i++) {
        plugin = [plugins objectAtIndex:i];
        mimes = [[plugin MIMETypeEnumerator] allObjects];
        if ([plugin isKindOfClass:[WebNetscapePluginPackage class]]){
            mimeCount = [mimes count];
            for (j = 0; j < mimeCount; j++){
                mime = [mimes objectAtIndex:j];
                
                // Don't override previously registered types.
                if(![viewTypes containsObject:mime]){
                    // Cocoa plugins must register themselves.
                    [WebView registerViewClass:[WebNetscapePluginDocumentView class] representationClass:[WebNetscapePluginRepresentation class] forMIMEType:mime];
                }
            }
        }
        else {
            if (!pendingPluginLoads)
                pendingPluginLoads = [[NSMutableArray alloc] init];
            [pendingPluginLoads addObject: plugin];
        }
    }

    return self;
}

- (void)loadPluginIfNeededForMIMEType: (NSString *)MIMEType
{
    NSArray *mimes;
    WebBasePluginPackage *plugin;
    int i, pluginCount;
    
    pluginCount = [pendingPluginLoads count];
    for (i = pluginCount-1; i >= 0; i--){
        plugin = [pendingPluginLoads objectAtIndex:i];
        mimes = [[plugin MIMETypeEnumerator] allObjects];
        if ([mimes containsObject: MIMEType]){
            [[plugin bundle] load];
            [pendingPluginLoads removeObject: plugin];
            continue;
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
