//
//  WebNSWorkspaceExtras.m
//  WebKit
//
//  Created by Chris Blumenberg on Wed Dec 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebNSWorkspaceExtras.h"

#import <WebFoundation/WebAssertions.h>

@implementation NSWorkspace (WebNSWorkspaceExtras)

- (void)_web_noteFileChangedAtPath:(NSString *)path
{
    ASSERT(path);
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    
    NSString *directoryPath = [path stringByDeletingLastPathComponent];
    const char *dirRep = [fileManager fileSystemRepresentationWithPath:directoryPath];

    // Send the notificaition that directory contents have changed.
    FNNotifyByPath(dirRep, kFNDirectoryModifiedMessage, kNilOptions);

    // Send a Finder event so the file attributes (including icon) update in the Finder.

    // Make the Finder the target of the event.
    OSType finderSignature = 'MACS';
    NSAppleEventDescriptor *target;
    target = [NSAppleEventDescriptor descriptorWithDescriptorType:typeApplSignature
                                                            bytes:&finderSignature
                                                           length:sizeof(OSType)];

    // Make the event.
    NSAppleEventDescriptor *event;
    event = [NSAppleEventDescriptor appleEventWithEventClass:kAEFinderSuite
                                                     eventID:kAESync
                                            targetDescriptor:target
                                                    returnID:0
                                               transactionID:0];

    const char *fileRep = [[NSFileManager defaultManager] fileSystemRepresentationWithPath:path];
    FSRef fref;
    OSStatus error = FSPathMakeRef(fileRep, &fref, NULL);
    if(error){
        return;
    }

    AliasHandle	alias;
    error = FSNewAlias(NULL, &fref, &alias);
    if(error){
        return;
    }

    // Make the file the direct object of the event.
    NSAppleEventDescriptor *aliasDesc;
    aliasDesc = [NSAppleEventDescriptor descriptorWithDescriptorType:typeAlias
                                                               bytes:*alias
                                                              length:GetHandleSize((Handle)alias)];
    [event setParamDescriptor:aliasDesc forKeyword:keyDirectObject];
    
    DisposeHandle((Handle)alias);

    // Since we don't care about the result, send the event without waiting for a reply.
    // This allows us to continue without having to block until we receive the reply.
    AESendMessage([event aeDesc], NULL, kAENoReply, 0);
}


@end
