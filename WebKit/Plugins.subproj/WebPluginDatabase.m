/*
        WebPluginDatabase.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/


#import <WebKit/WebBasePluginPackage.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNetscapePluginRepresentation.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginPackage.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>


@implementation WebPluginDatabase

static WebPluginDatabase *database = nil;

+ (WebPluginDatabase *)installedPlugins 
{
    if (!database) {
        database = [[WebPluginDatabase alloc] init];
    }
    return database;
}

- (WebBasePluginPackage *)pluginForKey:(NSString *)key withEnumeratorSelector:(SEL)enumeratorSelector
{
    WebBasePluginPackage *plugin, *CFMPlugin=nil, *machoPlugin=nil;
    uint i;

    for(i=0; i<[plugins count]; i++){
        plugin = [plugins objectAtIndex:i];
        if([[[plugin performSelector:enumeratorSelector] allObjects] containsObject:key]){
            if([plugin isKindOfClass:[WebPluginPackage class]]){
                // It's the new kind of plug-in. Return it immediately.
                return plugin;
            }else if([plugin isKindOfClass:[WebNetscapePluginPackage class]]){
                WebExecutableType executableType = [(WebNetscapePluginPackage *)plugin executableType];

                if(executableType == WebCFMExecutableType){
                    CFMPlugin = plugin;
                }else if(executableType == WebMachOExecutableType){
                    machoPlugin = plugin;
                }else{
                    [NSException raise:NSInternalInconsistencyException
                                format:@"Unknown executable type for plugin"];
                }
            }else{
                [NSException raise:NSInternalInconsistencyException
                            format:@"Unknown plugin class: %@", [plugin className]];
            }
        }
    }

    if(machoPlugin){
        return machoPlugin;
    }else{
        return CFMPlugin;
    }
}

- (WebBasePluginPackage *)pluginForMIMEType:(NSString *)MIMEType
{
    return [self pluginForKey:MIMEType withEnumeratorSelector:@selector(MIMETypeEnumerator)];
}

- (WebBasePluginPackage *)pluginForExtension:(NSString *)extension
{
    return [self pluginForKey:extension withEnumeratorSelector:@selector(extensionEnumerator)];
}

- (WebBasePluginPackage *)pluginForFilename:(NSString *)filename
{
    uint i;
    WebBasePluginPackage *plugin;
    
    for(i=0; i<[plugins count]; i++){
        plugin = [plugins objectAtIndex:i];
        if([[plugin filename] isEqualToString:filename]){
            return plugin;
        }
    }
    return nil;
}

- (NSArray *)plugins
{
    return plugins;
}

- (NSArray *)MIMETypes
{
    NSMutableSet *MIMETypes;
    WebBasePluginPackage *plugin;
    uint i;
        
    MIMETypes = [NSMutableSet set];
    for(i=0; i<[plugins count]; i++){
        plugin = [plugins objectAtIndex:i];
        [MIMETypes addObjectsFromArray:[[plugin MIMETypeEnumerator] allObjects]];
    }
    return [MIMETypes allObjects];
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
    uint i, n;
    
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
    NSArray *viewTypes = [[WebView _viewTypes] allKeys];
    NSArray *mimes = [self MIMETypes];
    NSString *mime;
    
    for (i = 0; i < [mimes count]; i++) {
        mime = [mimes objectAtIndex:i];
        
        // Don't override previously registered types.
        if(![viewTypes containsObject:mime]){
            // FIXME: This won't work for the new plug-ins.
            [WebView registerViewClass:[WebNetscapePluginDocumentView class] forMIMEType:mime];
            [WebDataSource registerRepresentationClass:[WebNetscapePluginRepresentation class] forMIMEType:mime];
        }
    }

    return self;
}

- (void)dealloc
{
    [plugins release];
    [super dealloc];
}

@end
