/*
 * Copyright (C) 2006, 2007 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#import "config.h"
#import "FileChooser.h"

#import "Document.h"
#import "SimpleFontData.h"
#import "Frame.h"
#import "Icon.h"
#import "LocalizedStrings.h"
#import "StringTruncator.h"
#import "WebCoreFrameBridge.h"

using namespace WebCore;

@interface WebCoreOpenPanelController : NSObject <WebCoreOpenPanelResultListener> {
    FileChooser *_fileChooser;
    WebCoreFrameBridge *_bridge;
}
- (id)initWithFileChooser:(FileChooser *)fileChooser;
- (void)disconnectFileChooser;
- (void)beginSheetWithFrame:(Frame*)frame;
@end

@implementation WebCoreOpenPanelController

- (id)initWithFileChooser:(FileChooser *)fileChooser
{
    self = [super init];
    if (!self)
        return nil;

    _fileChooser = fileChooser;
    return self;
}

- (void)disconnectFileChooser
{
    _fileChooser = 0;
}

- (void)beginSheetWithFrame:(Frame*)frame
{
    if (!_fileChooser)
        return;
    
    _bridge = frame->bridge();
    [_bridge retain];
    [_bridge runOpenPanelForFileButtonWithResultListener:self];
}

- (void)chooseFilename:(NSString *)filename
{
    if (_fileChooser)
        _fileChooser->chooseFile(filename);
    [_bridge release];
}

- (void)cancel
{
    [_bridge release];
}

@end

namespace WebCore {
    
FileChooser::FileChooser(FileChooserClient* client, const String& filename)
    : m_client(client)
    , m_filename(filename)
    , m_icon(chooseIcon(filename))
    , m_controller(AdoptNS, [[WebCoreOpenPanelController alloc] initWithFileChooser:this])
{
}

FileChooser::~FileChooser()
{
    [m_controller.get() disconnectFileChooser];
}

void FileChooser::openFileChooser(Document* document)
{
    if (Frame* frame = document->frame())
        [m_controller.get() beginSheetWithFrame:frame];
}

String FileChooser::basenameForWidth(const Font& font, int width) const
{
    if (width <= 0)
        return String();

    String strToTruncate;
    if (m_filename.isEmpty())
        strToTruncate = fileButtonNoFileSelectedLabel();
    else
        strToTruncate = [[NSFileManager defaultManager] displayNameAtPath:m_filename];

    return StringTruncator::centerTruncate(strToTruncate, width, font, false);
}

}
