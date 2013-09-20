/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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
 */

#include "config.h"
#include "RenderElement.h"

#include "ContentData.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderImage.h"
#include "RenderImageResourceStyleImage.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderMultiColumnBlock.h"
#include "RenderRegion.h"
#include "RenderRuby.h"
#include "RenderRubyText.h"
#include "RenderTableCaption.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableRow.h"
#include "RenderText.h"
#include "SVGRenderSupport.h"

namespace WebCore {

RenderElement::RenderElement(Element* element)
    : RenderObject(element)
{
}

RenderElement::~RenderElement()
{
}

RenderElement* RenderElement::createFor(Element& element, RenderStyle& style)
{
    Document& document = element.document();
    RenderArena& arena = *document.renderArena();

    // Minimal support for content properties replacing an entire element.
    // Works only if we have exactly one piece of content and it's a URL.
    // Otherwise acts as if we didn't support this feature.
    const ContentData* contentData = style.contentData();
    if (contentData && !contentData->next() && contentData->isImage() && !element.isPseudoElement()) {
        RenderImage* image = new (arena) RenderImage(&element);
        // RenderImageResourceStyleImage requires a style being present on the image but we don't want to
        // trigger a style change now as the node is not fully attached. Moving this code to style change
        // doesn't make sense as it should be run once at renderer creation.
        image->setStyleInternal(&style);
        if (const StyleImage* styleImage = static_cast<const ImageContentData*>(contentData)->image()) {
            image->setImageResource(RenderImageResourceStyleImage::create(const_cast<StyleImage*>(styleImage)));
            image->setIsGeneratedContent();
        } else
            image->setImageResource(RenderImageResource::create());
        image->setStyleInternal(0);
        return image;
    }

    if (element.hasTagName(HTMLNames::rubyTag)) {
        if (style.display() == INLINE)
            return new (arena) RenderRubyAsInline(element);
        if (style.display() == BLOCK)
            return new (arena) RenderRubyAsBlock(element);
    }
    // treat <rt> as ruby text ONLY if it still has its default treatment of block
    if (element.hasTagName(HTMLNames::rtTag) && style.display() == BLOCK)
        return new (arena) RenderRubyText(element);
    if (document.cssRegionsEnabled() && style.isDisplayRegionType() && !style.regionThread().isEmpty())
        return new (arena) RenderRegion(&element, 0);
    switch (style.display()) {
    case NONE:
        return 0;
    case INLINE:
        return new (arena) RenderInline(&element);
    case BLOCK:
    case INLINE_BLOCK:
    case RUN_IN:
    case COMPACT:
        if ((!style.hasAutoColumnCount() || !style.hasAutoColumnWidth()) && document.regionBasedColumnsEnabled())
            return new (arena) RenderMultiColumnBlock(element);
        return new (arena) RenderBlockFlow(&element);
    case LIST_ITEM:
        return new (arena) RenderListItem(element);
    case TABLE:
    case INLINE_TABLE:
        return new (arena) RenderTable(&element);
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
        return new (arena) RenderTableSection(&element);
    case TABLE_ROW:
        return new (arena) RenderTableRow(&element);
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
        return new (arena) RenderTableCol(element);
    case TABLE_CELL:
        return new (arena) RenderTableCell(&element);
    case TABLE_CAPTION:
        return new (arena) RenderTableCaption(element);
    case BOX:
    case INLINE_BOX:
        return new (arena) RenderDeprecatedFlexibleBox(element);
    case FLEX:
    case INLINE_FLEX:
        return new (arena) RenderFlexibleBox(&element);
    case GRID:
    case INLINE_GRID:
        return new (arena) RenderGrid(element);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

void RenderElement::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    ASSERT(children());
    RenderObjectChildList& children = *this->children();

    bool needsTable = false;

    if (newChild->isRenderTableCol()) {
        RenderTableCol* newTableColumn = toRenderTableCol(newChild);
        bool isColumnInColumnGroup = newTableColumn->isTableColumn() && isRenderTableCol();
        needsTable = !isTable() && !isColumnInColumnGroup;
    } else if (newChild->isTableCaption())
        needsTable = !isTable();
    else if (newChild->isTableSection())
        needsTable = !isTable();
    else if (newChild->isTableRow())
        needsTable = !isTableSection();
    else if (newChild->isTableCell())
        needsTable = !isTableRow();

    if (needsTable) {
        RenderTable* table;
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : children.lastChild();
        if (afterChild && afterChild->isAnonymous() && afterChild->isTable() && !afterChild->isBeforeContent())
            table = toRenderTable(afterChild);
        else {
            table = RenderTable::createAnonymousWithParentRenderer(this);
            addChild(table, beforeChild);
        }
        table->addChild(newChild);
    } else
        children.insertChildNode(this, newChild, beforeChild);

    if (newChild->isText() && newChild->style()->textTransform() == CAPITALIZE)
        toRenderText(newChild)->transformText();

    // SVG creates renderers for <g display="none">, as SVG requires children of hidden
    // <g>s to have renderers - at least that's how our implementation works. Consider:
    // <g display="none"><foreignObject><body style="position: relative">FOO...
    // - requiresLayer() would return true for the <body>, creating a new RenderLayer
    // - when the document is painted, both layers are painted. The <body> layer doesn't
    //   know that it's inside a "hidden SVG subtree", and thus paints, even if it shouldn't.
    // To avoid the problem alltogether, detect early if we're inside a hidden SVG subtree
    // and stop creating layers at all for these cases - they're not used anyways.
    if (newChild->hasLayer() && !layerCreationAllowedForSubtree())
        toRenderLayerModelObject(newChild)->layer()->removeOnlyThisLayer();

#if ENABLE(SVG)
    SVGRenderSupport::childAdded(this, newChild);
#endif
}

void RenderElement::removeChild(RenderObject* oldChild)
{
    ASSERT(children());
    children()->removeChildNode(this, oldChild);
}


}
