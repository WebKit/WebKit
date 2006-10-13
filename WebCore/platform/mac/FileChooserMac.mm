/*
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#import "config.h"
#import "FileChooser.h"

#import <AppKit/NSOpenPanel.h>
#import "Document.h"
#import "FontData.h"
#import "FrameMac.h"
#import "Icon.h"
#import "LocalizedStrings.h"
#import "RenderFileUploadControl.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreStringTruncator.h"

using namespace WebCore;

@interface OpenPanelController : NSObject <WebCoreOpenPanelResultListener> {
    FileChooser *_fileChooser;
    WebCoreFrameBridge *_bridge;
}
- (id)initWithFileChooser:(FileChooser *)fileChooser;
- (void)disconnectFileChooser;
- (void)beginSheet;
@end

@implementation OpenPanelController

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

- (void)beginSheet
{
    if (!_fileChooser)
        return;
    
    _bridge = Mac(_fileChooser->document()->frame())->bridge();
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
    
PassRefPtr<FileChooser> FileChooser::create(Document* document, RenderFileUploadControl* uploadControl)
{
    return new FileChooser(document, uploadControl);
}

FileChooser::FileChooser(Document* document, RenderFileUploadControl* uploadControl)
    : m_document(document)
    , m_icon(0)
    , m_uploadControl(uploadControl)
    , m_controller([[OpenPanelController alloc] initWithFileChooser:this])
{
}

FileChooser::~FileChooser()
{
    [m_controller release];
}

void FileChooser::openFileChooser()
{
    [m_controller beginSheet];
}

String FileChooser::basenameForWidth(int width) const
{
    if (width <= 0)
        return String();
    
    String strToTruncate;
    if (m_filename.isEmpty())
        strToTruncate = fileButtonNoFileSelectedLabel();
    else
        strToTruncate = [[NSFileManager defaultManager] displayNameAtPath:m_filename];
    
    return [WebCoreStringTruncator centerTruncateString:strToTruncate
            toWidth:width withFont:m_uploadControl->style()->font().primaryFont()->getNSFont()];
}

void FileChooser::disconnectUploadControl()
{
    if (m_controller)
        [m_controller disconnectFileChooser];
}

void FileChooser::chooseFile(const String& filename)
{
    if (m_filename == filename)
        return;
    
    m_filename = filename;
    
    // Need unsigned 0 here to disambiguate String::operator[] from operator(NSString*, int)[]
    if (!m_filename.length() || m_filename[0U] != '/')
        m_icon = 0;
    else
        m_icon = Icon::newIconForFile(m_filename);
    
    uploadControl()->valueChanged();
}

}
