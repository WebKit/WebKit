/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
              (C) 2004 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 2000-2003 Lars Knoll (knoll@kde.org)
              (C) 2000 Antti Koivisto (koivisto@kde.org)
              (C) 2000-2003 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Computer, Inc.

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

#ifndef KDOM_RenderStyleDefs_H
#define KDOM_RenderStyleDefs_H

/*
 * WARNING:
 * --------
 *
 * The order of the values in the enums have to agree with the order specified
 * in cssvalues.in, otherwise some optimizations in the parser will fail,
 * and produce invalid results.
 */

#include <qcolor.h>
#include <qpalette.h>

#include <kdom/Shared.h>
#include <kdom/css/impl/Font.h>
#include <kdom/impl/DOMStringImpl.h>
#include <kdom/css/impl/CounterImpl.h>
#include <kdom/css/impl/CSSValueImpl.h>
#include <kdom/cache/KDOMCachedImage.h>

// Helper macros for 'RenderStyle'
#define RS_DEFINE_ATTRIBUTE(Data, Type, Name, Initial) \
	void set##Type(Data val) { noninherited_flags.f._##Name = val; } \
	Data Name() const { return (Data) noninherited_flags.f._##Name; } \
	static Data initial##Type() { return Initial; }

#define RS_DEFINE_ATTRIBUTE_INHERITED(Data, Type, Name, Initial) \
	void set##Type(Data val) { inherited_flags.f._##Name = val; } \
	Data Name() const { return (Data) inherited_flags.f._##Name; } \
	static Data initial##Type() { return Initial; }

#define RS_DEFINE_ATTRIBUTE_DATAREF(Data, Group, Variable, Type, Name) \
	Data Name() const { return Group->Variable; } \
	void set##Type(Data obj) { RS_SET_VARIABLE(Group, Variable, obj) }

#define RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Data, Group, Variable, Type, Name, Initial) \
	RS_DEFINE_ATTRIBUTE_DATAREF(Data, Group, Variable, Type, Name) \
	static Data initial##Type() { return Initial; }

#define RS_SET_VARIABLE(Group, Variable, Value) \
	if(!(Group->Variable == Value)) \
		Group.access()->Variable = Value;

namespace KDOM
{
	template<class T>
	class DataRef
	{
	public:
		DataRef() { data = 0; }
		DataRef(const DataRef<T> &other) { data = other.data; data->ref(); }
		~DataRef() { if(data) data->deref(); }

		const T *operator->() const { return data; }
		const T *get() const { return data; }

		void init() { data = new T(); data->ref(); }

		T *access() const
		{
			if(data->refCount() != 1)
			{
				data->deref();

				data = new T(*data);
				data->ref();
			}

			return data;
		}

		DataRef<T> &operator=(const DataRef<T> &other)
		{
			if(data == other.data)
				return *this;

			if(data)
				data->deref();

			data = other.data;
			data->ref();

			return *this;
		}

		bool operator==(const DataRef<T> &other) const
		{
			return (*data == *(other.data));
		}

		bool operator!=(const DataRef<T> &other) const
		{
			return (*data != *(other.data) );
		}

	private:
		mutable T *data;
	};

	// Length attributes handling
	const int UNDEFINED = -1;
	enum LengthType { LT_VARIABLE = 0, LT_RELATIVE, LT_PERCENT, LT_FIXED, LT_STATIC };

	struct Length
	{
		Length() : _length(0) { }
		Length(LengthType t) { l.value = 0; l.type = t; l.quirk = 0; }
		Length(int v, LengthType t, bool q = false) { l.value = v; l.type = t; l.quirk = q; }

		bool operator==(const Length &other) const { return _length == other._length; }
		bool operator!=(const Length &other) const { return _length != other._length; }

		int width(int maxWidth) const
		{
			switch(l.type)
			{
				case LT_FIXED:
					return l.value;
				case LT_PERCENT:
					return maxWidth * (l.value / 100);
				case LT_VARIABLE:
					return maxWidth;
				default:
					return -1;
			}
		}

		int minWidth(int maxWidth) const
		{
			switch(l.type)
			{
				case LT_FIXED:
					return l.value;
				case LT_PERCENT:
					return maxWidth * (l.value / 100);
				case LT_VARIABLE:
				default:
					return 0;
			}
		}

		bool isVariable() const { return ((LengthType) l.type == LT_VARIABLE); }
		bool isRelative() const { return ((LengthType) l.type == LT_RELATIVE); }
		bool isPercent() const { return ((LengthType ) l.type == LT_PERCENT); }
		bool isFixed() const { return ((LengthType) l.type == LT_FIXED); }
		bool isStatic() const { return ((LengthType) l.type == LT_STATIC); }
		bool isQuirk() const { return l.quirk; }

		int value() const { return l.value; }
		LengthType type() const { return (LengthType) l.type; }

		union
		{
			struct
			{
				signed int value : 28;
				unsigned type : 3;
				bool quirk : 1;
			} l;

			Q_UINT32 _length;
		};
	};

	// Box model attributes. Not inherited.

	struct LengthBox
	{
		LengthBox() { }
		LengthBox(LengthType t) : left(t), right(t), top(t), bottom(t) { }

		Length &operator=(Length &other)
		{
			left = other; right = other;
			top = other; bottom = other;
			return other;
		}

		bool operator==(const LengthBox &other) const
		{
			return (left == other.left) && (right == other.right) &&
				   (top == other.top) && (bottom == other.bottom);
		}

		bool nonZero() const
		{
			return left.value() || right.value() ||
				   top.value() || bottom.value();
		}

		Length left, right;
		Length top, bottom;
	};

	// Border attributes. Not inherited.

	// These have been defined in the order of their precedence
	// for border-collapsing. Do not change this order!
	enum EBorderStyle { BS_NATIVE, BS_NONE, BS_HIDDEN, BS_INSET,
						BS_GROOVE, BS_RIDGE, BS_OUTSET, BS_DOTTED,
						BS_DASHED, BS_SOLID, BS_DOUBLE };

	class BorderValue
	{
	public:
    	BorderValue() : width(3), style(BS_NONE) { }
		~BorderValue() { }

		bool nonZero() const { return (width != 0) && !(style == BS_NONE); }
		bool isTransparent() const { return color.isValid() && qAlpha(color.rgb()) == 0; }

		bool operator==(const BorderValue &other) const
		{
			return (width == other.width) &&
				   (style == other.style) &&
				   (color == other.color);
		}

		QColor color;
		unsigned short width : 12;
		EBorderStyle style : 5;
	};

	class OutlineValue : public BorderValue
	{
	public:
		OutlineValue()
		{
			_offset = 0;
			_auto = false;
		}

		int _offset;
		bool _auto;
	};

	enum EBorderPrecedence { BP_OFF, BP_TABLE, BP_COLGROUP, BP_COL, BP_ROWGROUP, BP_ROW, BP_CELL };

	struct CollapsedBorderValue
	{
		CollapsedBorderValue() : border(0), precedence(BP_OFF) { }
		CollapsedBorderValue(const BorderValue *b, EBorderPrecedence p) : border(b), precedence(p) { }

		int width() const { return border && border->nonZero() ? border->width : 0; }
		EBorderStyle style() const { return border ? border->style : BS_HIDDEN; }
		bool exists() const { return border; }
		QColor color() const { return border ? border->color : QColor(); }
		bool isTransparent() const { return border ? border->isTransparent() : true; }

		bool operator==(const CollapsedBorderValue &other) const
		{
			if(!border)
				return !other.border;

			if(!other.border)
				return false;

			return (*border == *other.border) &&
				   (precedence == other.precedence);
		}

		const BorderValue *border;
		EBorderPrecedence precedence;
	};

	class BorderData : public Shared
	{
	public:
		BorderData() : Shared(false) { }
		virtual ~BorderData() { }

		bool hasBorder() const
		{
			return left.nonZero() || right.nonZero() ||
				   top.nonZero() || bottom.nonZero();
		}

		bool operator==(const BorderData &other) const
		{
			return (left == other.left) && (right == other.right) &&
				   (top == other.top) && (bottom == other.bottom);
		}

		BorderValue left, right;
		BorderValue top, bottom;
	};

	class StyleSurroundData : public Shared
	{
	public:
		StyleSurroundData();
		StyleSurroundData(const StyleSurroundData &other);

		bool operator==(const StyleSurroundData &other) const;
		bool operator!=(const StyleSurroundData &other) const
		{
			return !(*this == other);
		}

		LengthBox offset;
		LengthBox margin;
		LengthBox padding;

		BorderData border;

	private:
		StyleSurroundData &operator=(const StyleSurroundData &);
	};

	// Box attributes. Not inherited.

	enum EBoxSizing { BS_BORDER_BOX, BS_CONTENT_BOX };

	class StyleBoxData : public Shared
	{
	public:
		StyleBoxData();
		StyleBoxData(const StyleBoxData &other);
		virtual ~StyleBoxData() { }

		bool operator==(const StyleBoxData &other) const;
		bool operator!=(const StyleBoxData &other) const { return !(*this == other); }

		Length width;
		Length height;

		Length minWidth;
		Length maxWidth;

		Length minHeight;
		Length maxHeight;

		Length verticalAlign;

		EBoxSizing boxSizing;

		signed int zIndex : 31;
		bool zAuto : 1;

	private:
		StyleBoxData &operator=(const StyleBoxData &);
	};

	// Random visual rendering model attributes. Not inherited.

	class StyleVisualData : public Shared
	{
	public:
		StyleVisualData();
		StyleVisualData(const StyleVisualData &other);
		virtual ~StyleVisualData() { }

		bool operator==(const StyleVisualData &other) const;
		bool operator!=(const StyleVisualData &other) const { return !(*this == other); }

		LengthBox clip;
		unsigned textDecoration : 4; // Text decorations defined *only* by this element.

		QPalette palette;      //widget styling with IE attributes

	private:
		StyleVisualData &operator=(const StyleVisualData &);
	};

	enum EBackgroundRepeat { BR_REPEAT, BR_REPEAT_X, BR_REPEAT_Y, BR_NO_REPEAT };

	class StyleBackgroundData : public Shared
	{
	public:
		StyleBackgroundData();
		StyleBackgroundData(const StyleBackgroundData &other);
		virtual ~StyleBackgroundData() { }

		bool operator==(const StyleBackgroundData &other) const;
		bool operator!=(const StyleBackgroundData &other) const { return !(*this == other); }

		QColor color;
		CachedImage *image;

		Length xPosition;
		Length yPosition;
		OutlineValue outline;

	private:
		StyleBackgroundData &operator=(const StyleBackgroundData &);
	};

	// CSS3 marquee atrributes
	enum EMarqueeBehavior
	{
		MB_NONE, MB_SCROLL, MB_SLIDE, MB_ALTERNATE, MB_UNFURL
	};

	enum EMarqueeDirection
	{
		MD_AUTO = 0,
		MD_LEFT = 1,
		MD_RIGHT = -1,
		MD_UP = 2,
		MD_DOWN = -2,
		MD_FORWARD = 3,
		MD_BACKWARD = -3
	};

	class StyleMarqueeData : public Shared
	{
	public:
		StyleMarqueeData();
		StyleMarqueeData(const StyleMarqueeData &other);
		virtual ~StyleMarqueeData() { }

		bool operator==(const StyleMarqueeData &other) const;
		bool operator!=(const StyleMarqueeData &other) const { return !(*this == other); }

		Length increment;
		int speed;

		int loops; // -1 means infinite.

		EMarqueeBehavior behavior : 3;
		EMarqueeDirection direction : 3;
	};

	// This struct holds information about shadows for the text-shadow and box-shadow properties.
	struct ShadowData
	{
		ShadowData(int _x, int _y, int _blur, const QColor &_color) : x(_x), y(_y), blur(_blur), color(_color), next(0) { }
		ShadowData(const ShadowData &o) : x(o.x), y(o.y), blur(o.blur), color(o.color) { next = o.next ? new ShadowData(*o.next) : 0; }
		~ShadowData() { delete next; }

		bool ShadowData::operator==(const ShadowData &other) const
		{
			if((next && !other.next) || (!next && other.next) ||
			   (next && other.next && *next != *other.next))
				return false;

			return x == other.x && y == other.y &&
				   blur == other.blur && color == other.color;
		}

		bool operator!=(const ShadowData &other) const { return !(*this == other); }

		int x;
		int y;
		int blur;
		QColor color;
		ShadowData *next;
	};

	// This struct is for rarely used non-inherited CSS3 properties.  By grouping them together,
	// we save space, and only allocate this object when someone actually uses
	// a non-inherited CSS3 property.
	class StyleCSS3NonInheritedData : public Shared
	{
	public:
		StyleCSS3NonInheritedData();
		StyleCSS3NonInheritedData(const StyleCSS3NonInheritedData &other);

		virtual ~StyleCSS3NonInheritedData() { }

		bool operator==(const StyleCSS3NonInheritedData &other) const;
		bool operator!=(const StyleCSS3NonInheritedData &other) const { return !(*this == other); }

		float opacity; // Whether or not we're transparent.
		DataRef<StyleMarqueeData> marquee; // Marquee properties
	};

	// This struct is for rarely used inherited CSS3 properties.  By grouping them together,
	// we save space, and only allocate this object when someone actually uses
	// an inherited CSS3 property.
	class StyleCSS3InheritedData : public Shared
	{
	public:
		StyleCSS3InheritedData();
		StyleCSS3InheritedData(const StyleCSS3InheritedData &other);
		virtual ~StyleCSS3InheritedData();

		bool operator==(const StyleCSS3InheritedData &other) const;
		bool operator!=(const StyleCSS3InheritedData &other) const { return !(*this == other); }

		bool shadowDataEquivalent(const StyleCSS3InheritedData &other) const;

		ShadowData *textShadow;  // Our text shadow information for shadowed text drawing.

	private:
		StyleCSS3InheritedData &operator=(const StyleCSS3InheritedData &);
	};

	// Inherited attributes.

	enum EPageBreak { PB_AUTO, PB_ALWAYS, PB_AVOID };

	class StyleInheritedData : public Shared
	{
	public:
		StyleInheritedData();
		StyleInheritedData(const StyleInheritedData &other);
		virtual ~StyleInheritedData() { if(quotes) quotes->deref(); }

		bool operator==(const StyleInheritedData &other) const;
		bool operator!=(const StyleInheritedData &other) const { return !(*this == other); }

		Length indent;
		Length lineHeight;

		CachedImage *styleImage;

		Font font;
		QColor color;
		QColor decorationColor;

		short borderHSpacing;
		short borderVSpacing;

		QuotesValueImpl *quotes;

		// Paged media properties.
		short widows;
		short orphans;
		EPageBreak pageBreakInside : 2;

	private:
		StyleInheritedData &operator=(const StyleInheritedData &);
	};

	// Niscellaneous
	enum EQuoteContent
	{
		QC_NO_QUOTE, QC_OPEN_QUOTE, QC_CLOSE_QUOTE,
		QC_NO_OPEN_QUOTE, QC_NO_CLOSE_QUOTE
	};

	enum ContentType
	{
		CONTENT_NONE, CONTENT_OBJECT, CONTENT_TEXT,
		CONTENT_ATTR, CONTENT_COUNTER, CONTENT_QUOTE
	};

	struct ContentData
	{
		ContentData() : _contentType(CONTENT_NONE), _nextContent(0) { }
		~ContentData() { clearContent(); }

		void clearContent()
		{
			delete _nextContent; _nextContent = 0;
			switch (_contentType)
			{
				case CONTENT_OBJECT:
					_content.object = 0;
					break;
				case CONTENT_TEXT:
					_content.text->deref();
					_content.text = 0;
					break;
				case CONTENT_COUNTER:
					_content.counter->deref();
					_content.counter = 0;
					break;
				default: ;
			}
		}

		ContentType _contentType;

		DOMStringImpl *contentText() { if (_contentType == CONTENT_TEXT) return _content.text; return 0; }
		CachedObject *contentObject() { if (_contentType == CONTENT_OBJECT) return _content.object; return 0; }
		CounterImpl *contentCounter() { if (_contentType == CONTENT_COUNTER) return _content.counter; return 0; }
		EQuoteContent contentQuote() { if (_contentType == CONTENT_QUOTE) return _content.quote; return QC_NO_QUOTE; }

		union
		{
			CachedObject *object;
			DOMStringImpl *text;
			CounterImpl *counter;
			EQuoteContent quote;
		} _content;

		ContentData *_nextContent;
	};

	// Enumeration collection
	enum EOverflow
	{
		OF_VISIBLE, OF_HIDDEN, OF_SCROLL, OF_AUTO, OF_MARQUEE
	};

	enum EUnicodeBidi
	{
		UB_NORMAL, UB_EMBED, UB_OVERRIDE
	};

	enum EPosition
	{
		PS_STATIC, PS_RELATIVE, PS_ABSOLUTE, PS_FIXED
	};

	enum EFloat
	{
		FL_NONE, FL_LEFT, FL_RIGHT
	};

	enum EDirection
	{
		DI_LTR, DI_RTL
	};

	enum EVisibility
	{
		VS_VISIBLE, VS_HIDDEN, VS_COLLAPSE
	};

	enum ECursor
	{
		CS_AUTO, CS_CROSS, CS_DEFAULT, CS_POINTER,
		CS_PROGRESS, CS_MOVE, CS_E_RESIZE, CS_NE_RESIZE,
		CS_NW_RESIZE, CS_N_RESIZE, CS_SE_RESIZE, CS_SW_RESIZE,
		CS_S_RESIZE, CS_W_RESIZE, CS_TEXT, CS_WAIT, CS_HELP
	};

	enum EUserInput
	{
		UI_ENABLED, UI_DISABLED, UI_NONE
	};

	enum EDisplay
	{
		DS_INLINE, DS_BLOCK, DS_LIST_ITEM, DS_RUN_IN,
		DS_COMPACT, DS_INLINE_BLOCK, DS_TABLE, DS_INLINE_TABLE,
		DS_TABLE_ROW_GROUP, DS_TABLE_HEADER_GROUP, DS_TABLE_FOOTER_GROUP,
		DS_TABLE_ROW, DS_TABLE_COLUMN_GROUP, DS_TABLE_COLUMN, DS_TABLE_CELL,
		DS_TABLE_CAPTION, DS_NONE
	};

	enum EVerticalAlign
	{
		VA_BASELINE, VA_MIDDLE, VA_SUB, VA_SUPER, VA_TEXT_TOP,
		VA_TEXT_BOTTOM, VA_TOP, VA_BOTTOM, VA_BASELINE_MIDDLE, VA_LENGTH
	};

	enum EClear
	{
		CL_NONE, CL_LEFT, CL_RIGHT, CL_BOTH
	};

	enum ETableLayout
	{
		TL_AUTO, TL_FIXED
	};

	enum EEmptyCell
	{
		EC_SHOW, EC_HIDE
	};

	enum ECaptionSide
	{
		CS_TOP, CS_BOTTOM, CS_LEFT, CS_RIGHT
	};

	enum EListStyleType
	{
		// Symbols:
		LT_DISC, LT_CIRCLE, LT_SQUARE, LT_BOX, LT_DIAMOND,
		// Numeric:
		LT_DECIMAL, LT_DECIMAL_LEADING_ZERO, LT_ARABIC_INDIC, LT_LAO, LT_PERSIAN, LT_URDU, LT_THAI, LT_TIBETAN,
		// Algorithmic:
		LT_LOWER_ROMAN, LT_UPPER_ROMAN, LT_HEBREW, LT_ARMENIAN, LT_GEORGIAN,
		// Ideographic:
		LT_CJK_IDEOGRAPHIC, LT_JAPANESE_FORMAL, LT_JAPANESE_INFORMAL,
		LT_SIMP_CHINESE_FORMAL, LT_SIMP_CHINESE_INFORMAL, LT_TRAD_CHINESE_FORMAL, LT_TRAD_CHINESE_INFORMAL,
		// Alphabetic:
		LT_LOWER_GREEK, LT_UPPER_GREEK, LT_LOWER_ALPHA, LT_LOWER_LATIN, LT_UPPER_ALPHA, LT_UPPER_LATIN,
		LT_HIRAGANA, LT_KATAKANA, LT_HIRAGANA_IROHA, LT_KATAKANA_IROHA,
		// Special:
		LT_OPEN_QUOTE, LT_CLOSE_QUOTE, LT_NONE
	};

	enum EListStylePosition
	{
		LP_OUTSIDE, LP_INSIDE
	};

	enum EWhiteSpace
	{
		WS_NORMAL, WS_PRE, WS_NOWRAP, WS_PRE_WRAP, WS_PRE_LINE, WS_KHTML_NOWRAP
	};

	enum ETextAlign
	{
		TA_AUTO, TA_LEFT, TA_RIGHT, TA_CENTER, TA_JUSTIFY, TA_KHTML_LEFT, TA_KHTML_RIGHT, TA_KHTML_CENTER
	};

	enum ETextTransform
	{
		TT_CAPITALIZE, TT_UPPERCASE, TT_LOWERCASE, TT_NONE
	};

	// Special types
	enum ETextDecoration
	{
		TD_NONE = 0x0,
		TD_UNDERLINE = 0x1,
		TD_OVERLINE = 0x2,
		TD_LINE_THROUGH= 0x4,
		TD_BLINK = 0x8
	};
};

#endif

// vim:ts=4:noet
