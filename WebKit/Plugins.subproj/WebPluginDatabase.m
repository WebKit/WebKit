/*
        WebPluginDatabase.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBasePluginPackage.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebNetscapePluginRepresentation.h>
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

// The first plugin with the specified mime type is returned.
- (WebBasePluginPackage *)pluginForMIMEType:(NSString *)MIME
{
    WebBasePluginPackage *plugin;
    uint i;
    
    for(i=0; i<[plugins count]; i++){      
        plugin = [plugins objectAtIndex:i];
        if([[plugin MIMEToExtensionsDictionary] objectForKey:MIME]){
            return plugin;
        }
    }
    return nil;
}

- (WebBasePluginPackage *)pluginForExtension:(NSString *)extension
{
    WebBasePluginPackage *plugin;
    uint i;

    for(i=0; i<[plugins count]; i++){
        plugin = [plugins objectAtIndex:i];
        if([[plugin extensionToMIMEDictionary] objectForKey:extension]){
            return plugin;
        }
    }
    return nil;
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
        [MIMETypes addObjectsFromArray:[[plugin MIMEToDescriptionDictionary] allKeys]];
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
    WebBasePluginPackage *pluginPackage;
    
    for (i = 0; i < [pluginPaths count]; i++) {
        pluginPackage = [WebBasePluginPackage pluginWithPath:[pluginPaths objectAtIndex:i]];
        if (pluginPackage) {
            [pluginArray addObject:pluginPackage];
            LOG(Plugins, "Found plugin: %s", [[pluginPackage name] lossyCString]);
            LOG(Plugins, "%s", [[pluginPackage description] lossyCString]);
            [pluginPackage release];
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
