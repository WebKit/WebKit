/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"
#include "FileChooser.h"

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore {

FileChooser::FileChooser(Document*, RenderFileUploadControl*)
{
    notImplemented();
}

FileChooser::~FileChooser()
{
    notImplemented();
}

PassRefPtr<FileChooser> FileChooser::create(Document*, RenderFileUploadControl*)
{
    notImplemented();
    return 0;
}

void FileChooser::openFileChooser()
{
    notImplemented();
}

String FileChooser::basenameForWidth(int width) const
{
    notImplemented();
    return String();
}

void FileChooser::disconnectUploadControl()
{
    notImplemented();
}

void FileChooser::chooseFile(const String& filename)
{
    notImplemented();
}

}

// vim: ts=4 sw=4 et
