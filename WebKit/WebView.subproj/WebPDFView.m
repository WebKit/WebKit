/*	
    WebPDFView.m
    Copyright 2004, Apple, Inc. All rights reserved.
*/
// Assume we'll only ever compile this on Panther or greater, so 
// MAC_OS_X_VERSION_10_3 is guaranateed to be defined.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3

#import <Foundation/NSString_NSURLExtras.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebPDFView.h>


@implementation WebPDFView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
            written = NO;
    }
    return self;
}

- (void)dealloc
{
    [path release];
    [super dealloc];
}

#define TEMP_PREFIX "/tmp/XXXXXX-"
#define OBJC_TEMP_PREFIX @"/tmp/XXXXXX-"

static void applicationInfoForMIMEType(NSString *type, NSString **name, NSImage **image)
{
    NSURL *appURL = nil;
    
    OSStatus error = LSCopyApplicationForMIMEType((CFStringRef)type, kLSRolesAll, (CFURLRef *)&appURL);
    if(error != noErr){
        return;
    }

    NSString *appPath = [appURL path];
    CFRelease (appURL);
    
    *image = [[NSWorkspace sharedWorkspace] iconForFile:appPath];  
    [*image setSize:NSMakeSize(16.f,16.f)];  
    
    NSString *appName = [[NSFileManager defaultManager] displayNameAtPath:appPath];
    *name = appName;
}


- (NSString *)path
{
    // Generate path once.
    if (path)
        return path;
        
    NSString *filename = [[dataSource response] suggestedFilename];
    NSFileManager *manager = [NSFileManager defaultManager];    
    
    path = [@"/tmp/" stringByAppendingPathComponent: filename];
    if ([manager fileExistsAtPath:path]) {
        path = [OBJC_TEMP_PREFIX stringByAppendingString:filename];
        char *cpath = (char *)[path fileSystemRepresentation];
        
        int fd = mkstemps (cpath, strlen(cpath) - strlen(TEMP_PREFIX) + 1);
        if (fd < 0) {
            // Couldn't create a temporary file!  Should never happen.  Do
            // we need an alert here?
            path = nil;
        }
        else {
            close (fd);
            path = [manager stringWithFileSystemRepresentation:cpath length:strlen(cpath)];
        }
    }
    
    [path retain];
    
    return path;
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
    NSMenu *menu = [super menuForEvent:theEvent];
    NSString *appName = nil;
    NSImage *appIcon = nil;
    
    applicationInfoForMIMEType([[dataSource response] MIMEType], &appName, &appIcon);
    if (!appName)
        appName = UI_STRING("Finder", "Default application name for Open With context menu");
    
    NSString *title = [NSString stringWithFormat:UI_STRING("Open with %@", "Open document using the Finder"), appName];
    
    NSMenuItem *item = [menu insertItemWithTitle:title action:@selector(openWithFinder:) keyEquivalent:@"" atIndex:0];
    if (appIcon)
        [item setImage:appIcon];
        
    [menu insertItem:[NSMenuItem separatorItem] atIndex:1];
    
    return menu;
}

- (void)setDataSource:(WebDataSource *)ds
{
    dataSource = ds;
    [self setFrame:[[self superview] frame]];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

- (void)setNeedsLayout:(BOOL)flag
{
}

- (void)layout
{
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
}

- (void)viewDidMoveToHostWindow
{
}

- (void)openWithFinder:(id)sender
{
    NSString *opath = [self path];
    
    if (opath) {
        if (!written) {
            [[dataSource data] writeToFile:opath atomically:YES];
            written = YES;
        }
    
        if (![[NSWorkspace sharedWorkspace] openFile:opath]) {
            // NSWorkspace couldn't open file.  Do we need an alert
            // here?  We ignore the error elsewhere.
        }
    }
}

- (BOOL)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag wrap: (BOOL)wrapFlag;
{
    BOOL lastFindWasSuccessful = NO;
    
    // FIXME:  Insert find code here when ready.
    
    return lastFindWasSuccessful;
}


@end

#endif //MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
