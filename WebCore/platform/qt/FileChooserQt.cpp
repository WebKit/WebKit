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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "FileChooser.h"

#include "Document.h"
#include "Frame.h"
#include "FrameLoaderClientQt.h"
#include "Icon.h"
#include "Page.h"

#include <QFontMetrics>

namespace WebCore {

void FileChooser::openFileChooser(Document* doc)
{
    Page *page = doc->page();
    Frame *frame = doc->frame();
    if (!page || !frame)
        return;

    FrameLoaderClientQt *fl = static_cast<FrameLoaderClientQt*>(frame->loader()->client());
    if (!fl)
        return;

    QString f = fl->chooseFile(m_filenames[0]);
    if (!f.isEmpty())
        chooseFile(f);
}

String FileChooser::basenameForWidth(const Font& f, int width) const
{
    QFontMetrics fm(f.font());
    return fm.elidedText(m_filenames[0], Qt::ElideLeft, width);
}

}

// vim: ts=4 sw=4 et
