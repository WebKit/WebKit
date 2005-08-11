/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Computer, Inc.

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <qfont.h>

#include "RenderStyle.h"

using namespace KDOM;

RenderStyle *RenderStyle::s_defaultStyle = 0;

RenderStyle::RenderStyle() : KDOM::Shared(false)
{
	if(!s_defaultStyle)
		s_defaultStyle = new RenderStyle(true);

	box = s_defaultStyle->box;
	visual = s_defaultStyle->visual;
	surround = s_defaultStyle->surround;
	background = s_defaultStyle->background;
	css3InheritedData = s_defaultStyle->css3InheritedData;
	css3NonInheritedData = s_defaultStyle->css3NonInheritedData;

	inherited = s_defaultStyle->inherited;

	setBitDefaults();
	
	pseudoStyle = 0;
	content = 0;
	counter_reset = 0;
	counter_increment = 0;
}

RenderStyle::RenderStyle(bool) : KDOM::Shared(false)
{
	setBitDefaults();
	
	box.init();
	visual.init();
	surround.init();
	background.init();
	css3InheritedData.init();
	css3NonInheritedData.init();
	css3NonInheritedData.access()->marquee.init();

	inherited.init();
	pseudoStyle = 0;
	content = 0;
	counter_reset = 0;
	counter_increment = 0;
}

RenderStyle::RenderStyle(const RenderStyle &other) : KDOM::Shared(false)
{
	inherited_flags = other.inherited_flags;
	noninherited_flags = other.noninherited_flags;

	box = other.box;
	visual = other.visual;
	surround = other.surround;
	background = other.background;
	css3InheritedData = other.css3InheritedData;
	css3NonInheritedData = other.css3NonInheritedData;

	inherited = other.inherited;
	
	pseudoStyle = 0;
	
	content = other.content;
	counter_reset = other.counter_reset;
	counter_increment = other.counter_increment;

	if(counter_reset)
		counter_reset->ref();
		
	if(counter_increment)
		counter_increment->ref();
}

RenderStyle::~RenderStyle()
{
	RenderStyle *ps = pseudoStyle;
	RenderStyle *prev = 0;

	while(ps)
	{
		prev = ps;
		ps = ps->pseudoStyle;
		
		// to prevent a double deletion.
		// this works only because the styles below aren't really shared
		// Dirk said we need another construct as soon as these are shared
		prev->pseudoStyle = 0;
		prev->deref();
	}
	
	delete content;
	
	if(counter_reset)
		counter_reset->deref();
		
	if(counter_increment)
		counter_increment->deref();
}

void RenderStyle::cleanup()
{
	delete s_defaultStyle;
	s_defaultStyle = 0;
}

bool RenderStyle::equals(RenderStyle *other) const
{
	// compare everything except the pseudoStyle pointer
	return (inherited_flags == other->inherited_flags) &&
		   (noninherited_flags == other->noninherited_flags) &&
		   (box == other->box) && (visual == other->visual) &&
		   (surround == other->surround) && (background == other->background) &&
		   (inherited == other->inherited) && (css3InheritedData == other->css3InheritedData) &&
		   (css3NonInheritedData == other->css3NonInheritedData);
}

void RenderStyle::inheritFrom(const RenderStyle *inheritParent)
{
	inherited = inheritParent->inherited;
	css3InheritedData = inheritParent->css3InheritedData;

	inherited_flags = inheritParent->inherited_flags;

	// Simulate ":after,:before { white-space: pre-line }"
	if(noninherited_flags.f._styleType == AFTER ||
	   noninherited_flags.f._styleType == BEFORE)
		setWhiteSpace(WS_PRE_LINE);
}

RenderStyle *RenderStyle::getPseudoStyle(PseudoId pid)
{
	RenderStyle *ps = 0;
	if(noninherited_flags.f._styleType == NOPSEUDO)
	{
		for(ps = pseudoStyle; ps != 0; ps = ps->pseudoStyle)
		{
			if(ps->noninherited_flags.f._styleType == pid)
				break;
		}
	}

	return ps;
}

RenderStyle *RenderStyle::addPseudoStyle(PseudoId pid)
{
	RenderStyle *ps = getPseudoStyle(pid);

	if(!ps)
	{
		switch(pid)
		{
			case FIRST_LETTER: // pseudo-elements (FIRST_LINE has a special handling)
			case BEFORE:
			case AFTER:
			{
				ps = new RenderStyle();
				break;
			}
			default:
				ps = new RenderStyle(*this); // use the real copy constructor to get an identical copy
		}
		
		ps->ref();
		ps->noninherited_flags.f._styleType = pid;
		ps->pseudoStyle = pseudoStyle;

		pseudoStyle = ps;
	}

	return ps;
}

void RenderStyle::removePseudoStyle(PseudoId pid)
{
	RenderStyle *ps = pseudoStyle;
	RenderStyle *prev = this;

	while(ps)
	{
		if(ps->noninherited_flags.f._styleType == pid)
		{
			prev->pseudoStyle = ps->pseudoStyle;
			ps->deref();
			return;
		}
		
		prev = ps;
		ps = ps->pseudoStyle;
	}
}

bool RenderStyle::inheritedNotEqual(RenderStyle *other) const
{
	return (inherited_flags != other->inherited_flags) ||
		   (inherited != other->inherited) ||
		   (css3InheritedData != other->css3InheritedData);
}

/*
  compares two styles. The result gives an idea of the action that
  needs to be taken when replacing the old style with a new one.

  CbLayout: The containing block of the object needs a relayout.
  Layout: the RenderObject needs a relayout after the style change.
  Visible: The change is visible, but no relayout is needed.
  NonVisible: The object does need neither repaint nor relayout after the change.

  ### TODO:
  A lot can be optimised here based on the display type, lots of
  optimisations are unimplemented, and currently result in the
  worst case result causing a relayout of the containing block.
*/
RenderStyle::Diff RenderStyle::diff(const RenderStyle *other) const
{
	if(*box.get() != *other->box.get() ||
	   *visual.get() != *other->visual.get() ||
	   *surround.get() != *other->surround.get() ||
	   !(inherited->indent == other->inherited->indent) ||
	   !(inherited->lineHeight == other->inherited->lineHeight) ||
	   !(inherited->styleImage == other->inherited->styleImage) ||
	   !(inherited->font == other->inherited->font) ||
	   !(inherited->borderHSpacing == other->inherited->borderHSpacing) ||
	   !(inherited->borderVSpacing == other->inherited->borderVSpacing))
		return CbLayout;

	if(((int) noninherited_flags.f._display) >= DS_TABLE)
	{
		if(!(inherited_flags.f._emptyCells == other->inherited_flags.f._emptyCells) ||
		   !(inherited_flags.f._captionSide == other->inherited_flags.f._captionSide) ||
		   !(inherited_flags.f._borderCollapse == other->inherited_flags.f._borderCollapse) ||
		   !(noninherited_flags.f._tableLayout == other->noninherited_flags.f._tableLayout) ||
		   !(noninherited_flags.f._position == other->noninherited_flags.f._position) ||
		   !(noninherited_flags.f._floating == other->noninherited_flags.f._floating) ||
		   !(noninherited_flags.f._unicodeBidi == other->noninherited_flags.f._unicodeBidi) )
			return CbLayout;
	}

	if(noninherited_flags.f._display == DS_LIST_ITEM)
	{
		if(!(inherited_flags.f._listStyleType == other->inherited_flags.f._listStyleType) ||
		   !(inherited_flags.f._listStylePosition == other->inherited_flags.f._listStylePosition))
		   return Layout;
	}
	
	if(!(inherited_flags.f._textAlign == other->inherited_flags.f._textAlign) ||
	   !(inherited_flags.f._textTransform == other->inherited_flags.f._textTransform) ||
	   !(inherited_flags.f._direction == other->inherited_flags.f._direction) ||
	   !(inherited_flags.f._whiteSpace == other->inherited_flags.f._whiteSpace) ||
	   !(noninherited_flags.f._clear == other->noninherited_flags.f._clear))
		return Layout;
	
	if(!(noninherited_flags.f._display == DS_INLINE) &&
	   !(noninherited_flags.f._verticalAlign == other->noninherited_flags.f._verticalAlign))
		return Layout;

	if(inherited->color != other->inherited->color ||
	   inherited->decorationColor != other->inherited->decorationColor ||
	   !(inherited_flags.f._visibility == other->inherited_flags.f._visibility) ||
	   !(noninherited_flags.f._overflow == other->noninherited_flags.f._overflow) ||
	   !(noninherited_flags.f._backgroundRepeat == other->noninherited_flags.f._backgroundRepeat) ||
	   !(noninherited_flags.f._backgroundAttachment == other->noninherited_flags.f._backgroundAttachment) ||
	   !(inherited_flags.f._textDecoration == other->inherited_flags.f._textDecoration) ||
	   !(noninherited_flags.f._hasClip == other->noninherited_flags.f._hasClip) ||
	   visual->textDecoration != other->visual->textDecoration ||
	   *background.get() != *other->background.get() ||
	   css3NonInheritedData->opacity != other->css3NonInheritedData->opacity ||
	   !css3InheritedData->shadowDataEquivalent(*other->css3InheritedData.get()))
		return Visible;

	return Equal;
}

void RenderStyle::setTextShadow(ShadowData *val, bool add)
{
	StyleCSS3InheritedData *css3Data = css3InheritedData.access();
	if(!add)
	{
		delete css3Data->textShadow;
		css3Data->textShadow = val;
		return;
	}

	ShadowData *last = css3Data->textShadow;
	while(last->next != 0)
		last = last->next;
	
	last->next = val;
}

void RenderStyle::setClip(Length top, Length right, Length bottom, Length left)
{
	StyleVisualData *data = visual.access();
	data->clip.top = top;
	data->clip.right = right;
	data->clip.bottom = bottom;
	data->clip.left = left;
}

void RenderStyle::setQuotes(QuotesValueImpl *q)
{
	QuotesValueImpl *t = inherited->quotes;
	inherited.access()->quotes = q;
	
	if(q)
		q->ref();
	
	if(t)
		t->deref();
}

QString RenderStyle::openQuote(int level) const
{
	if(inherited->quotes)
		return inherited->quotes->openQuote(level);

	return QString::fromLatin1("\""); // 0 is default quotes
}

QString RenderStyle::closeQuote(int level) const
{
	if(inherited->quotes)
			return inherited->quotes->closeQuote(level);
	
	return QString::fromLatin1("\""); // 0 is default quotes
}

bool RenderStyle::contentDataEquivalent(RenderStyle *otherStyle)
{
	ContentData *c1 = content;
	ContentData *c2 = otherStyle->content;

	while(c1 && c2)
	{
		if(c1->_contentType != c2->_contentType)
			return false;
			
		if(c1->_contentType == CONTENT_TEXT)
		{
			DOMString c1Str(c1->_content.text);
			DOMString c2Str(c2->_content.text);
			
			if(c1Str != c2Str)
				return false;
		}
		else if(c1->_contentType == CONTENT_OBJECT)
		{
			if(c1->_content.object != c2->_content.object)
				return false;
		}
		else if(c1->_contentType == CONTENT_COUNTER)
		{
			if(c1->_content.counter != c2->_content.counter)
				return false;
		}

		c1 = c1->_nextContent;
		c2 = c2->_nextContent;
	}

	return !c1 && !c2;
}

void RenderStyle::setContent(CachedObject *o, bool add)
{
	if(!o)
		return; // The object is null. Nothing to do. Just bail.

	ContentData *lastContent = content;
	while(lastContent && lastContent->_nextContent)
		lastContent = lastContent->_nextContent;

	bool reuseContent = !add;
	
	ContentData *newContentData = 0;
	if(reuseContent && content)
	{
		content->clearContent();
		newContentData = content;
	}
	else
		newContentData = new ContentData();

	if(lastContent && !reuseContent)
		lastContent->_nextContent = newContentData;
	else
		content = newContentData;

	//    o->ref();
	newContentData->_content.object = o;
	newContentData->_contentType = CONTENT_OBJECT;
}

void RenderStyle::setContent(DOMStringImpl *s, bool add)
{
	if(!s)
		return; // The string is null. Nothing to do. Just bail.

	ContentData *lastContent = content;
	while(lastContent && lastContent->_nextContent)
		lastContent = lastContent->_nextContent;

	bool reuseContent = !add;
	if(add)
	{
		if(!lastContent)
			return; // Something's wrong.  We had no previous content, and we should have.

		if(lastContent->_contentType == CONTENT_TEXT)
		{
			// We can augment the existing string and share this ContentData node.
			DOMStringImpl *oldStr = lastContent->_content.text;
			DOMStringImpl *newStr = oldStr->copy();
			newStr->ref();
			oldStr->deref();
			
			newStr->append(s);
			
			lastContent->_content.text = newStr;
			return;
		}
	}

	ContentData *newContentData = 0;
	if(reuseContent && content)
	{
		content->clearContent();
		newContentData = content;
	}
	else
		newContentData = new ContentData();

	if(lastContent && !reuseContent)
		lastContent->_nextContent = newContentData;
	else
		content = newContentData;

	newContentData->_content.text = s;
	newContentData->_content.text->ref();
	newContentData->_contentType = CONTENT_TEXT;
}

void RenderStyle::setContent(CounterImpl *c, bool add)
{
	if(!c)
		return;

	ContentData *lastContent = content;
	while(lastContent && lastContent->_nextContent)
		lastContent = lastContent->_nextContent;

	bool reuseContent = !add;
	ContentData *newContentData = 0;
	if(reuseContent && content)
	{
		content->clearContent();
		newContentData = content;
	}
	else
		newContentData = new ContentData();

	if(lastContent && !reuseContent)
		lastContent->_nextContent = newContentData;
	else
		content = newContentData;

	c->ref();
	newContentData->_content.counter = c;
	newContentData->_contentType = CONTENT_COUNTER;
}

void RenderStyle::setContent(EQuoteContent q, bool add)
{
	if(q == QC_NO_QUOTE)
		return;

	ContentData* lastContent = content;
	while(lastContent && lastContent->_nextContent)
		lastContent = lastContent->_nextContent;

	bool reuseContent = !add;
	ContentData *newContentData = 0;
	if(reuseContent && content)
	{
		content->clearContent();
		newContentData = content;
	}
	else
		newContentData = new ContentData();

	if(lastContent && !reuseContent)
		lastContent->_nextContent = newContentData;
	else
		content = newContentData;

	newContentData->_content.quote = q;
	newContentData->_contentType = CONTENT_QUOTE;
}

bool RenderStyle::counterDataEquivalent(RenderStyle *otherStyle)
{
	// ### Should we compare content?
	return counter_reset == otherStyle->counter_reset &&
		   counter_increment == otherStyle->counter_increment;
}

void RenderStyle::setCounterReset(CSSValueListImpl *l)
{
	CSSValueListImpl *t = counter_reset;
	counter_reset = l;

	if(l)
		l->ref();
	
	if(t)
		t->deref();
}

void RenderStyle::setCounterIncrement(CSSValueListImpl *l)
{
	CSSValueListImpl *t = counter_increment;
	counter_increment = l;

	if(l)
		l->ref();
		
	if(t)
		t->deref();
}

void RenderStyle::addCounterReset(CounterActImpl *c)
{
	if(!counter_reset)
	{
		counter_reset = new CSSValueListImpl();
		counter_reset->ref();
	}

	counter_reset->append(c);
}

void RenderStyle::addCounterIncrement(CounterActImpl *c)
{
	if(!counter_increment)
	{
		counter_increment = new CSSValueListImpl();
		counter_increment->ref();
	}

	counter_increment->append(c);
}

static bool hasCounter(const DOMString &c, CSSValueListImpl *l)
{
	int len = l->length();
	for(int i = 0; i < len; i++)
	{
		CounterActImpl *ca = static_cast<CounterActImpl *>(l->item(i));
		Q_ASSERT(ca != 0);
		
		if(ca->m_counter == c)
			return true;
	}

	return false;
}

bool RenderStyle::hasCounterReset(const DOMString &c) const
{
	if(counter_reset)
		return hasCounter(c, counter_reset);
	
	return false;
}

bool RenderStyle::hasCounterIncrement(const DOMString &c) const
{
	if(counter_increment)
		return hasCounter(c, counter_increment);
	
	return false;
}

static short readCounter(const DOMString &c, CSSValueListImpl *l)
{
	int len = l->length();
	for(int i = 0; i < len; i++)
	{
		CounterActImpl *ca = static_cast<CounterActImpl *>(l->item(i));
		Q_ASSERT(ca != 0);
		
		if(ca->m_counter == c)
			return ca->m_value;
	}
	
	return 0;
}

short RenderStyle::counterReset(const DOMString &c) const
{
	if(counter_reset)
		return readCounter(c, counter_reset);
	
	return 0;
}

short RenderStyle::counterIncrement(const DOMString &c) const
{
	if(counter_increment)
		return readCounter(c, counter_increment);
	
	return 0;
}

static QString describeFont(const QFont &f)
{
	QString res = QString::fromLatin1("'") + f.family() + QString::fromLatin1("' ");

#ifndef APPLE_COMPILE_HACK
	if(f.pointSize() > 0)
		res += QString::number(f.pointSize()) + QString::fromLatin1("pt");
	else
		res += QString::number(f.pixelSize()) + QString::fromLatin1("px");
#endif

	if(f.bold())
		res += QString::fromLatin1(" bold");
	if(f.italic())
#ifndef APPLE_COMPILE_HACK
		res += QString::fromLatin1(" italic");
	if(f.underline())
		res += QString::fromLatin1(" underline");
	if(f.overline())
		res += QString::fromLatin1(" overline");
	if(f.strikeOut())
#endif
		res += QString::fromLatin1(" strikeout");
	
	return res;
}

QString RenderStyle::createDiff(const RenderStyle &parent) const
{
	QString res;
	if(color().isValid() && parent.color() != color())
		res += QString::fromLatin1(" [color=") + color().name() + QString::fromLatin1("]");
	if(backgroundColor().isValid() && parent.backgroundColor() != backgroundColor())
		res += QString::fromLatin1(" [bgcolor=") + backgroundColor().name() + QString::fromLatin1("]");
	if(parent.font() != font())
		res += QString::fromLatin1(" [font=") + describeFont(font()) + QString::fromLatin1("]");
	
	return res;
}

// vim:ts=4:noet
