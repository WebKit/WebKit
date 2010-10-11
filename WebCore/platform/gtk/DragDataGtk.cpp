/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DragData.h"

#include "Clipboard.h"
#include "ClipboardGtk.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "markup.h"

namespace WebCore {

bool DragData::canSmartReplace() const
{
    return false;
}

bool DragData::containsColor() const
{
    return false;
}

bool DragData::containsFiles() const
{
    return m_platformDragData->hasFilenames();
}

void DragData::asFilenames(Vector<String>& result) const
{
    result = m_platformDragData->filenames();
}

bool DragData::containsPlainText() const
{
    return m_platformDragData->hasText();
}

String DragData::asPlainText() const
{
    return m_platformDragData->text();
}

Color DragData::asColor() const
{
    return Color();
}

bool DragData::containsCompatibleContent() const
{
    return containsPlainText() || containsURL() || m_platformDragData->hasMarkup() || containsColor() || containsFiles();
}

bool DragData::containsURL(FilenameConversionPolicy filenamePolicy) const
{
    return m_platformDragData->hasURL();
}

String DragData::asURL(FilenameConversionPolicy filenamePolicy, String* title) const
{
    String url(m_platformDragData->url());
    if (title)
        *title = m_platformDragData->urlLabel();
    return url;
}


PassRefPtr<DocumentFragment> DragData::asFragment(Document* document) const
{
    if (!m_platformDragData->hasMarkup())
        return 0;

    return createFragmentFromMarkup(document, m_platformDragData->markup(), "");
}

}
