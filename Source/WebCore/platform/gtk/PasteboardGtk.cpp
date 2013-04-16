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

#include "ClipboardGtk.h"
#include "DataObjectGtk.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "GOwnPtrGtk.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "TextResourceDecoder.h"
#include "Image.h"
#include "RenderImage.h"
#include "markup.h"
#include <gtk/gtk.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

#if ENABLE(SVG)
#include "SVGNames.h"
#include "XLinkNames.h"
#endif

namespace WebCore {

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard();
    return pasteboard;
}

Pasteboard::Pasteboard()
{
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    PasteboardHelper* helper = PasteboardHelper::defaultPasteboardHelper();
    GtkClipboard* clipboard = helper->getClipboard(frame);

    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    dataObject->clearAll();

    dataObject->setText(frame->editor()->selectedText());
    dataObject->setMarkup(createMarkup(selectedRange, 0, AnnotateForInterchange, false, ResolveNonLocalURLs));
    helper->writeClipboardContents(clipboard, canSmartCopyOrDelete ? PasteboardHelper::IncludeSmartPaste : PasteboardHelper::DoNotIncludeSmartPaste);
}

void Pasteboard::writePlainText(const String& text)
{
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    dataObject->clearAll();

    dataObject->setText(text);
    PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(clipboard);
}

void Pasteboard::writeURL(const KURL& url, const String& label, Frame* frame)
{
    if (url.isEmpty())
        return;

    PasteboardHelper* helper = PasteboardHelper::defaultPasteboardHelper();
    GtkClipboard* clipboard = helper->getClipboard(frame);

    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    dataObject->clearAll();

    dataObject->setURL(url, label);
    helper->writeClipboardContents(clipboard);
}

static KURL getURLForImageNode(Node* node)
{
    // FIXME: Later this code should be shared with Chromium somehow. Chances are all platforms want it.
    AtomicString urlString;
    if (node->hasTagName(HTMLNames::imgTag) || node->hasTagName(HTMLNames::inputTag))
        urlString = static_cast<Element*>(node)->getAttribute(HTMLNames::srcAttr);
#if ENABLE(SVG)
    else if (node->hasTagName(SVGNames::imageTag))
        urlString = static_cast<Element*>(node)->getAttribute(XLinkNames::hrefAttr);
#endif
    else if (node->hasTagName(HTMLNames::embedTag) || node->hasTagName(HTMLNames::objectTag)) {
        Element* element = static_cast<Element*>(node);
        urlString = element->getAttribute(element->imageSourceAttributeName());
    }
    return urlString.isEmpty() ? KURL() : node->document()->completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
}

void Pasteboard::writeImage(Node* node, const KURL&, const String& title)
{
    ASSERT(node);

    if (!(node->renderer() && node->renderer()->isImage()))
        return;

    RenderImage* renderer = toRenderImage(node->renderer());
    CachedImage* cachedImage = renderer->cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return;
    Image* image = cachedImage->imageForRenderer(renderer);
    ASSERT(image);

    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    dataObject->clearAll();

    KURL url = getURLForImageNode(node);
    if (!url.isEmpty()) {
        dataObject->setURL(url, title);

        dataObject->setMarkup(createMarkup(static_cast<Element*>(node), IncludeNode, 0, ResolveAllURLs));
    }

    GRefPtr<GdkPixbuf> pixbuf = adoptGRef(image->getGdkPixbuf());
    dataObject->setImage(pixbuf.get());

    PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(clipboard);
}

void Pasteboard::writeClipboard(Clipboard* clipboard)
{
    GtkClipboard* gtkClipboard = static_cast<ClipboardGtk*>(clipboard)->clipboard();
    if (gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(gtkClipboard);
}

void Pasteboard::clear()
{
    gtk_clipboard_clear(gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD));
}

bool Pasteboard::canSmartReplace()
{
    return PasteboardHelper::defaultPasteboardHelper()->clipboardContentSupportsSmartReplace(
        gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD));
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context,
                                                          bool allowPlainText, bool& chosePlainText)
{
    PasteboardHelper* helper = PasteboardHelper::defaultPasteboardHelper();
    GtkClipboard* clipboard = helper->getCurrentClipboard(frame);
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    helper->getClipboardContents(clipboard);

    chosePlainText = false;

    if (dataObject->hasMarkup()) {
        RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(frame->document(), dataObject->markup(), "", FragmentScriptingAndPluginContentNotAllowed);
        if (fragment)
            return fragment.release();
    }

    if (!allowPlainText)
        return 0;

    if (dataObject->hasText()) {
        chosePlainText = true;
        RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), dataObject->text());
        if (fragment)
            return fragment.release();
    }

    return 0;
}

String Pasteboard::plainText(Frame* frame)
{
    PasteboardHelper* helper = PasteboardHelper::defaultPasteboardHelper();
    GtkClipboard* clipboard = helper->getCurrentClipboard(frame);
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);

    helper->getClipboardContents(clipboard);
    return dataObject->text();
}

}
