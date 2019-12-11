/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "ExtensionManagerWindowController.h"

#import "AppDelegate.h"
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/_WKUserContentExtensionStore.h>

@implementation ExtensionManagerWindowController

- (instancetype)init
{
    self = [self initWithWindowNibName:@"ExtensionManagerWindowController"];
    if (self) {
        NSArray* installedContentExtensions = [[NSUserDefaults standardUserDefaults] arrayForKey:@"InstalledContentExtensions"];
        if (installedContentExtensions) {
            for (NSString *identifier in installedContentExtensions) {
                [[_WKUserContentExtensionStore defaultStore] lookupContentExtensionForIdentifier:identifier completionHandler:^(_WKUserContentFilter *extension, NSError *error)
                {
                    if (error) {
                        NSLog(@"Extension store got out of sync with system defaults.");
                        return;
                    }

                    BrowserAppDelegate* appDelegate = (BrowserAppDelegate *)[[NSApplication sharedApplication] delegate];
                    [appDelegate.userContentContoller _addUserContentFilter:extension];
                }];
            }
                
        }
    }
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    NSArray* installedContentExtensions = [[NSUserDefaults standardUserDefaults] arrayForKey:@"InstalledContentExtensions"];
    if (installedContentExtensions) {
        for (NSString *extension in installedContentExtensions)
            [arrayController addObject:extension];
    }
}

- (IBAction)add:(id)sender
{
    NSOpenPanel *openPanel = [[NSOpenPanel openPanel] retain];
    openPanel.allowedFileTypes = @[ @"public.json" ];
    
    [openPanel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result)
    {
        if (result != NSModalResponseOK)
            return;

        NSURL *url = [openPanel.URLs objectAtIndex:0];
        NSString *identifier = url.lastPathComponent;
        NSString *jsonString = [[NSString alloc] initWithContentsOfURL:url encoding:NSUTF8StringEncoding error:nil];

        [[_WKUserContentExtensionStore defaultStore] compileContentExtensionForIdentifier:identifier encodedContentExtension:jsonString completionHandler:^(_WKUserContentFilter *extension, NSError *error)
        {
            
            if (error) {
                NSAlert *alert = [NSAlert alertWithError:error];
                [alert runModal];
                return;
            }

            NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

            NSArray* installedContentExtensions = [defaults arrayForKey:@"InstalledContentExtensions"];
            NSMutableArray *mutableInstalledContentExtensions;
            if (installedContentExtensions)
                mutableInstalledContentExtensions = [installedContentExtensions mutableCopy];
            else
                mutableInstalledContentExtensions = [[NSMutableArray alloc] init];

            [mutableInstalledContentExtensions addObject:identifier];
            [defaults setObject:mutableInstalledContentExtensions forKey:@"InstalledContentExtensions"];
            [mutableInstalledContentExtensions release];

            [arrayController addObject:identifier];

            BrowserAppDelegate* appDelegate = (BrowserAppDelegate *)[[NSApplication sharedApplication] delegate];
            [appDelegate.userContentContoller _addUserContentFilter:extension];
        }];
    }];

}

- (IBAction)remove:(id)sender
{
    NSUInteger index = [arrayController selectionIndex];

    NSString *identifierToRemove = arrayController.arrangedObjects[index];

    [[_WKUserContentExtensionStore defaultStore] removeContentExtensionForIdentifier:identifierToRemove completionHandler:^(NSError *error)
    {
        if (error) {
            NSAlert *alert = [NSAlert alertWithError:error];
            [alert runModal];
            return;
        }

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

        NSMutableArray *installedContentExtensions = [[defaults arrayForKey:@"InstalledContentExtensions"] mutableCopy];
        [installedContentExtensions removeObject:identifierToRemove];
        [defaults setObject:installedContentExtensions forKey:@"InstalledContentExtensions"];
        [installedContentExtensions release];

        [arrayController removeObjectAtArrangedObjectIndex:index];
        BrowserAppDelegate* appDelegate = (BrowserAppDelegate *)[[NSApplication sharedApplication] delegate];
        [appDelegate.userContentContoller _removeUserContentFilter:identifierToRemove];
    }];
}

@end
