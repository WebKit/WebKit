//
//  WebNSWorkspaceExtras.h
//  WebKit
//
//  Created by Chris Blumenberg on Wed Dec 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>


@interface NSWorkspace (WebNSWorkspaceExtras)

// Notifies the Finder (or any other app that cares) that we added, removed or changed the attributes of a file.
// This method is better than calling noteFileSystemChanged: because noteFileSystemChanged: sends 2 apple events
// both with a 60 second timeout. This method returns immediately.
- (void)_web_noteFileChangedAtPath:(NSString *)path;

@end
