/*	
    WebPDFView.m
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#ifndef OMIT_TIGER_FEATURES

#import <Foundation/NSString_NSURLExtras.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebPDFView.h>

#import <Quartz/Quartz.h>

NSString *_NSPathForSystemFramework(NSString *framework);

@implementation WebPDFView

+ (NSBundle *)PDFKitBundle
{
    static NSBundle *PDFKitBundle = nil;
    if (PDFKitBundle == nil) {
        NSString *PDFKitPath = [_NSPathForSystemFramework(@"Quartz.framework") stringByAppendingString:@"/Frameworks/PDFKit.framework"];
        if (PDFKitPath == nil) {
            ERROR("Couldn't find PDFKit.framework");
            return nil;
        }
        PDFKitBundle = [NSBundle bundleWithPath:PDFKitPath];
        if (![PDFKitBundle load]) {
            ERROR("Couldn't load PDFKit.framework");
        }
    }
    return PDFKitBundle;
}

+ (Class)PDFViewClass
{
    static Class PDFViewClass = nil;
    if (PDFViewClass == nil) {
        PDFViewClass = [[WebPDFView PDFKitBundle] classNamed:@"PDFView"];
        if (PDFViewClass == nil) {
            ERROR("Couldn't find PDFView class in PDFKit.framework");
        }
    }
    return PDFViewClass;
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
        PDFSubview = [[[[self class] PDFViewClass] alloc] initWithFrame:frame];
        [PDFSubview setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
        [self addSubview:PDFSubview];
        written = NO;
    }
    return self;
}

- (void)dealloc
{
    [PDFSubview release];
    [path release];
    [super dealloc];
}

- (PDFView *)PDFSubview
{
    return PDFSubview;
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

#endif // OMIT_TIGER_FEATURES
