/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther
 *  Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
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
#include "Pasteboard.h"

#include "CachedImage.h"
#include "DataObjectGtk.h"
#include "DocumentFragment.h"
#include "DragData.h"
#include "Editor.h"
#include "Frame.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "Image.h"
#include "URL.h"
#include "PasteboardHelper.h"
#include "RenderImage.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include "markup.h"
#include <gtk/gtk.h>


namespace WebCore {

enum ClipboardDataType {
    ClipboardDataTypeText,
    ClipboardDataTypeMarkup,
    ClipboardDataTypeURIList,
    ClipboardDataTypeURL,
    ClipboardDataTypeImage,
    ClipboardDataTypeUnknown
};

PassOwnPtr<Pasteboard> Pasteboard::create(GtkClipboard* gtkClipboard)
{
    return adoptPtr(new Pasteboard(gtkClipboard));
}

PassOwnPtr<Pasteboard> Pasteboard::create(PassRefPtr<DataObjectGtk> dataObject)
{
    return adoptPtr(new Pasteboard(dataObject));
}

PassOwnPtr<Pasteboard> Pasteboard::createForCopyAndPaste()
{
    return create(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
}

PassOwnPtr<Pasteboard> Pasteboard::createForGlobalSelection()
{
    return create(gtk_clipboard_get(GDK_SELECTION_PRIMARY));
}

PassOwnPtr<Pasteboard> Pasteboard::createPrivate()
{
    return create(DataObjectGtk::create());
}

#if ENABLE(DRAG_SUPPORT)
PassOwnPtr<Pasteboard> Pasteboard::createForDragAndDrop()
{
    return create(DataObjectGtk::create());
}

PassOwnPtr<Pasteboard> Pasteboard::createForDragAndDrop(const DragData& dragData)
{
    return create(dragData.platformData());
}
#endif

Pasteboard::Pasteboard(PassRefPtr<DataObjectGtk> dataObject)
    : m_dataObject(dataObject)
    , m_gtkClipboard(0)
{
    ASSERT(m_dataObject);
}

Pasteboard::Pasteboard(GtkClipboard* gtkClipboard)
    : m_dataObject(DataObjectGtk::forClipboard(gtkClipboard))
    , m_gtkClipboard(gtkClipboard)
{
    ASSERT(m_dataObject);
}

Pasteboard::~Pasteboard()
{
}

PassRefPtr<DataObjectGtk> Pasteboard::dataObject() const
{
    return m_dataObject;
}

static ClipboardDataType dataObjectTypeFromHTMLClipboardType(const String& rawType)
{
    String type(rawType.stripWhiteSpace());

    // Two special cases for IE compatibility
    if (type == "Text" || type == "text")
        return ClipboardDataTypeText;
    if (type == "URL")
        return ClipboardDataTypeURL;

    // From the Mac port: Ignore any trailing charset - JS strings are
    // Unicode, which encapsulates the charset issue.
    if (type == "text/plain" || type.startsWith("text/plain;"))
        return ClipboardDataTypeText;
    if (type == "text/html" || type.startsWith("text/html;"))
        return ClipboardDataTypeMarkup;
    if (type == "Files" || type == "text/uri-list" || type.startsWith("text/uri-list;"))
        return ClipboardDataTypeURIList;

    // Not a known type, so just default to using the text portion.
    return ClipboardDataTypeUnknown;
}

bool Pasteboard::writeString(const String& type, const String& data)
{
    switch (dataObjectTypeFromHTMLClipboardType(type)) {
    case ClipboardDataTypeURIList:
    case ClipboardDataTypeURL:
        m_dataObject->setURIList(data);
        return true;
    case ClipboardDataTypeMarkup:
        m_dataObject->setMarkup(data);
        return true;
    case ClipboardDataTypeText:
        m_dataObject->setText(data);
        return true;
    case ClipboardDataTypeUnknown:
        m_dataObject->setUnknownTypeData(type, data);
        return true;
    case ClipboardDataTypeImage:
        break;
    }

    return false;
}

void Pasteboard::writeSelection(Range& selectedRange, bool canSmartCopyOrDelete, Frame& frame, ShouldSerializeSelectedTextForClipboard shouldSerializeSelectedTextForClipboard)
{
    m_dataObject->clearAll();
    m_dataObject->setText(shouldSerializeSelectedTextForClipboard == IncludeImageAltTextForClipboard ? frame.editor().selectedTextForClipboard() : frame.editor().selectedText());
    m_dataObject->setMarkup(createMarkup(selectedRange, 0, AnnotateForInterchange, false, ResolveNonLocalURLs));

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard, canSmartCopyOrDelete ? PasteboardHelper::IncludeSmartPaste : PasteboardHelper::DoNotIncludeSmartPaste);
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption smartReplaceOption)
{
    m_dataObject->clearAll();
    m_dataObject->setText(text);

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard, (smartReplaceOption == CanSmartReplace) ? PasteboardHelper::IncludeSmartPaste : PasteboardHelper::DoNotIncludeSmartPaste);
}

void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    ASSERT(!pasteboardURL.url.isEmpty());

    m_dataObject->clearAll();
    m_dataObject->setURL(pasteboardURL.url, pasteboardURL.title);

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard);
}

static URL getURLForImageElement(Element& element)
{
    // FIXME: Later this code should be shared with Chromium somehow. Chances are all platforms want it.
    AtomicString urlString;
    if (isHTMLImageElement(element) || isHTMLInputElement(element))
        urlString = element.getAttribute(HTMLNames::srcAttr);
    else if (element.hasTagName(SVGNames::imageTag))
        urlString = element.getAttribute(XLinkNames::hrefAttr);
    else if (element.hasTagName(HTMLNames::embedTag) || isHTMLObjectElement(element))
        urlString = element.imageSourceURL();

    return urlString.isEmpty() ? URL() : element.document().completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
}

void Pasteboard::writeImage(Element& element, const URL&, const String& title)
{
    if (!(element.renderer() && element.renderer()->isRenderImage()))
        return;

    RenderImage* renderer = toRenderImage(element.renderer());
    CachedImage* cachedImage = renderer->cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return;
    Image* image = cachedImage->imageForRenderer(renderer);
    ASSERT(image);

    m_dataObject->clearAll();

    URL url = getURLForImageElement(element);
    if (!url.isEmpty()) {
        m_dataObject->setURL(url, title);

        m_dataObject->setMarkup(createMarkup(element, IncludeNode, 0, ResolveAllURLs));
    }

    GRefPtr<GdkPixbuf> pixbuf = adoptGRef(image->getGdkPixbuf());
    m_dataObject->setImage(pixbuf.get());

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard);
}

void Pasteboard::writePasteboard(const Pasteboard& sourcePasteboard)
{
    RefPtr<DataObjectGtk> sourceDataObject = sourcePasteboard.dataObject();
    m_dataObject->clearAll();

    if (sourceDataObject->hasText())
        m_dataObject->setText(sourceDataObject->text());
    if (sourceDataObject->hasMarkup())
        m_dataObject->setMarkup(sourceDataObject->markup());
    if (sourceDataObject->hasURL())
        m_dataObject->setURL(sourceDataObject->url(), sourceDataObject->urlLabel());
    if (sourceDataObject->hasURIList())
        m_dataObject->setURIList(sourceDataObject->uriList());
    if (sourceDataObject->hasImage())
        m_dataObject->setImage(sourceDataObject->image());
    if (sourceDataObject->hasUnknownTypeData()) {
        auto types = m_dataObject->unknownTypes();
        auto end = types.end();
        for (auto it = types.begin(); it != end; ++it)
            m_dataObject->setUnknownTypeData(it->key, it->value);
    }

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard);
}

void Pasteboard::clear()
{
    // We do not clear filenames. According to the spec: "The clearData() method
    // does not affect whether any files were included in the drag, so the types
    // attribute's list might still not be empty after calling clearData() (it would
    // still contain the "Files" string if any files were included in the drag)."
    m_dataObject->clearAllExceptFilenames();

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard);
}

void Pasteboard::clear(const String& type)
{
    switch (dataObjectTypeFromHTMLClipboardType(type)) {
    case ClipboardDataTypeURIList:
    case ClipboardDataTypeURL:
        m_dataObject->clearURIList();
        break;
    case ClipboardDataTypeMarkup:
        m_dataObject->clearMarkup();
        break;
    case ClipboardDataTypeText:
        m_dataObject->clearText();
        break;
    case ClipboardDataTypeImage:
        m_dataObject->clearImage();
        break;
    case ClipboardDataTypeUnknown:
        m_dataObject->clearAll();
        break;
    }

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard);
}

bool Pasteboard::canSmartReplace()
{
    return m_gtkClipboard && PasteboardHelper::defaultPasteboardHelper()->clipboardContentSupportsSmartReplace(m_gtkClipboard);
}

#if ENABLE(DRAG_SUPPORT)
void Pasteboard::setDragImage(DragImageRef, const IntPoint& hotSpot)
{
}
#endif

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame& frame, Range& context, bool allowPlainText, bool& chosePlainText)
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);

    chosePlainText = false;

    if (m_dataObject->hasMarkup()) {
        if (frame.document()) {
            RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(*frame.document(), m_dataObject->markup(), emptyString(), DisallowScriptingAndPluginContent);
            if (fragment)
                return fragment.release();
        }
    }

    if (!allowPlainText)
        return 0;

    if (m_dataObject->hasText()) {
        chosePlainText = true;
        RefPtr<DocumentFragment> fragment = createFragmentFromText(context, m_dataObject->text());
        if (fragment)
            return fragment.release();
    }

    return 0;
}

void Pasteboard::read(PasteboardPlainText& text)
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);
    text.text = m_dataObject->text();
}

bool Pasteboard::hasData()
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);

    return m_dataObject->hasText() || m_dataObject->hasMarkup() || m_dataObject->hasURIList() || m_dataObject->hasImage() || m_dataObject->hasUnknownTypeData();
}

Vector<String> Pasteboard::types()
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);

    Vector<String> types;
    if (m_dataObject->hasText()) {
        types.append(ASCIILiteral("text/plain"));
        types.append(ASCIILiteral("Text"));
        types.append(ASCIILiteral("text"));
    }

    if (m_dataObject->hasMarkup())
        types.append(ASCIILiteral("text/html"));

    if (m_dataObject->hasURIList()) {
        types.append(ASCIILiteral("text/uri-list"));
        types.append(ASCIILiteral("URL"));
    }

    if (m_dataObject->hasFilenames())
        types.append(ASCIILiteral("Files"));

    auto unknownTypes = m_dataObject->unknownTypes();
    auto end = unknownTypes.end();
    for (auto it = unknownTypes.begin(); it != end; ++it)
        types.append(it->key);

    return types;
}

String Pasteboard::readString(const String& type)
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);

    switch (dataObjectTypeFromHTMLClipboardType(type)) {
    case ClipboardDataTypeURIList:
        return m_dataObject->uriList();
    case ClipboardDataTypeURL:
        return m_dataObject->url();
    case ClipboardDataTypeMarkup:
        return m_dataObject->markup();
    case ClipboardDataTypeText:
        return m_dataObject->text();
    case ClipboardDataTypeUnknown:
        return m_dataObject->unknownTypeData(type);
    case ClipboardDataTypeImage:
        break;
    }

    return String();
}

Vector<String> Pasteboard::readFilenames()
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);

    return m_dataObject->filenames();
}

}
