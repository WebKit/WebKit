/*
        WebPluginDatabase.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDataSource.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginStream.h>
#import <WebKit/WebView.h>


@implementation WebNetscapePluginDatabase

static WebNetscapePluginDatabase *database = nil;

+ (WebNetscapePluginDatabase *)installedPlugins 
{
    if (!database) {
        database = [[WebNetscapePluginDatabase alloc] init];
    }
    return database;
}

// The first plugin with the specified mime type is returned.
- (WebNetscapePlugin *)pluginForMIMEType:(NSString *)MIME
{
    WebNetscapePlugin *plugin;
    uint i;
    
    for(i=0; i<[plugins count]; i++){      
        plugin = [plugins objectAtIndex:i];
        if([[plugin MIMEToExtensionsDictionary] objectForKey:MIME]){
            return plugin;
        }
    }
    return nil;
}

- (WebNetscapePlugin *)pluginForExtension:(NSString *)extension
{
    WebNetscapePlugin *plugin;
    uint i;

    for(i=0; i<[plugins count]; i++){
        plugin = [plugins objectAtIndex:i];
        if([[plugin extensionToMIMEDictionary] objectForKey:extension]){
            return plugin;
        }
    }
    return nil;
}

- (WebNetscapePlugin *)pluginForFilename:(NSString *)filename
{
    uint i;
    WebNetscapePlugin *plugin;
    
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
    WebNetscapePlugin *plugin;
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
    NSFileManager *fileManager;
    NSArray *pluginDirectories, *files;
    NSString *file;
    NSMutableArray *pluginPaths, *pluginArray;
    NSMutableSet *filenames;
    WebNetscapePlugin *plugin;
    uint i, n;
    
    self = [super init];
    if (self == nil) {
        return nil;
    }
    
    pluginDirectories = pluginLocations();
    
    fileManager = [NSFileManager defaultManager];

    pluginPaths = [NSMutableArray arrayWithCapacity:10];
    filenames = [NSMutableSet setWithCapacity:10];

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
    
    pluginArray = [NSMutableArray arrayWithCapacity:[pluginPaths count]];
    
    for (i = 0; i < [pluginPaths count]; i++) {
        plugin = [[WebNetscapePlugin alloc] initWithPath:[pluginPaths objectAtIndex:i]];
        if (plugin) {
            [pluginArray addObject:plugin];
            LOG(Plugins, "Found plugin: %s", [[plugin name] lossyCString]);
            LOG(Plugins, "%s", [[plugin description] lossyCString]);
            [plugin release];
        }
    }

    plugins = [pluginArray copy];

    // register plug-in WebDocumentViews and WebDocumentRepresentations
    NSArray *mimes = [self MIMETypes];
    for (i = 0; i < [mimes count]; i++) {
        [WebView registerViewClass:[WebNetscapePluginDocumentView class] forMIMEType:[mimes objectAtIndex:i]];
        [WebDataSource registerRepresentationClass:[WebNetscapePluginStream class] forMIMEType:[mimes objectAtIndex:i]];
    }

    return self;
}

- (void)dealloc
{
    [plugins release];
    [super dealloc];
}

@end
