/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#import "KWQKStandardDirs.h"

// The NSBundle calls in this file can't throw, so no need to block
// Cocoa exceptions.

@interface KWQKStandardDirsBundleDummy : NSObject { }
@end
@implementation KWQKStandardDirsBundleDummy
@end

QString locate(const char *type, const QString &filename, const KInstance *instance)
{
    // FIXME: Eliminate this hard-coding at some point?
    bool quirk = true;
    if (filename.contains("html4"))
        quirk = false;
    NSBundle *bundle = [NSBundle bundleForClass:[KWQKStandardDirsBundleDummy class]];
    if (quirk)
        return QString::fromNSString([bundle pathForResource:@"quirks" ofType:@"css"]);
    return QString::fromNSString([bundle pathForResource:@"html4" ofType:@"css"]);
}

QString locateLocal(const char *type, const QString &filename, const KInstance *instance)
{
    return QString();
}
