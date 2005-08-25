/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml css code by:
    Copyright (C) 2003 Lars Knoll (knoll@kde.org)

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

#ifndef KDOM_CSSPARSER_H
#define KDOM_CSSPARSER_H

#include <qstring.h>
#include <qptrlist.h>

#include <kdom/impl/DOMStringImpl.h>

namespace KDOM
{
	class CSSProperty;
	class CSSRuleImpl;
	class DocumentImpl;
	class CSSValueImpl;
	class CDFInterface;
	class StyleListImpl;
	class CSSValueListImpl;
	class CSSStyleRuleImpl;
	class CSSStyleSheetImpl;
	class CSSPrimitiveValueImpl;
	class CSSStyleDeclarationImpl;

	struct KDOMCSSParseString
	{
		unsigned short *string;
		int length;
	};

	class KDOMCSSValueList;

	struct KDOMCSSFunction
	{
		KDOMCSSParseString name;
		KDOMCSSValueList *args;
	};

	struct KDOMCSSValue
	{
		int id;
		union
		{
			double fValue;
			int iValue;
			KDOMCSSParseString string;
			struct KDOMCSSFunction *function;
		};
		enum
		{
			Operator = 0x100000,
			Function = 0x100001,
			Q_EMS     = 0x100002
		};
		int unit;
	};

	static inline QString qString(const KDOMCSSParseString &ps)
	{
		return QString((QChar *)ps.string, ps.length);
	}
	static inline DOMStringImpl *domString(const KDOMCSSParseString &ps)
	{
		return new DOMStringImpl((QChar *)ps.string, ps.length);
	}

	class KDOMCSSValueList
	{
	public:
		KDOMCSSValueList();
		~KDOMCSSValueList();
		void addValue(const KDOMCSSValue &val);
		KDOMCSSValue *current() { return currentValue < numValues ? values + currentValue : 0; }
		KDOMCSSValue *next() { ++currentValue; return current(); }
		bool isLast() const { return currentValue+1 >= numValues; }
		KDOMCSSValue *values;
		int numValues;
		int maxValues;
		int currentValue;
	};

	class CSSParser
	{
	public:
		CSSParser(bool strictParsing = true);
		virtual ~CSSParser();

		CDFInterface *interface() const;

		void parseSheet(CSSStyleSheetImpl *sheet, DOMStringImpl *string);
		CSSRuleImpl *parseRule(CSSStyleSheetImpl *sheet, DOMStringImpl *string);

		bool parseValue(CSSStyleDeclarationImpl *decls, int id,
						DOMStringImpl *string, bool _important, bool _nonCSSHint);

		bool parseDeclaration(CSSStyleDeclarationImpl *decls,
							  DOMStringImpl *string, bool _nonCSSHint);

		static CSSParser *current() { return currentParser; }

		DocumentImpl *document() const;

		void addProperty(int propId, CSSValueImpl *value, bool important);
		bool hasProperties() const { return numParsedProperties > 0; }
		void clearProperties();

		CSSStyleDeclarationImpl *createStyleDeclaration(CSSStyleRuleImpl *rule);

		virtual bool parseValue(int propId, bool important, int expected = 1);
		virtual bool parseShape(int propId, bool important);

		bool parseShortHand(const int *properties, int numProperties, bool important);
		bool parse4Values(const int *properties, bool important);
		bool parseContent(int propId, bool important);
		bool parseFont(bool important);
		bool parseCounter(int propId, bool increment, bool important);

		// returns the found property
		// 0 if nothing found (or ok == false)
		// @param forward if true, it parses the next in the list
		CSSPrimitiveValueImpl *parseBackgroundPositionXY(int propId, bool forward, bool &ok);
		CSSValueListImpl *parseFontFamily();
		CSSPrimitiveValueImpl *parseColor();
		CSSPrimitiveValueImpl *parseColorFromValue(KDOMCSSValue *value);
		CSSValueImpl *parseCounterContent(KDOMCSSValueList *args, bool counters);

		// CSS3 Parsing Routines (for properties specific to CSS3)
		bool parseShadow(int propId, bool important);

		static bool validUnit(KDOMCSSValue *value, int unitflags, bool strict);

		virtual CSSStyleDeclarationImpl *createCSSStyleDeclaration(CSSStyleRuleImpl *rule,
																   QPtrList<CSSProperty> *propList);

	public:
		bool strict;
		bool important;
		bool nonCSSHint;
		unsigned int id;
		StyleListImpl* styleElement;
		CSSRuleImpl *rule;
		KDOMCSSValueList *valueList;
		CSSProperty **parsedProperties;
		int numParsedProperties;
		int maxParsedProperties;
		bool inParseShortHand;
		unsigned int defaultNamespace;
		static CSSParser *currentParser;

	// tokenizer methods and data
	public:
		int lex(void *yylval);
		int token() { return yyTok; }
		unsigned short *text(int *length);
		int lex();

	private:
		int yyparse();
		void runParser(int length);

		unsigned short *data;
		unsigned short *yytext;
		unsigned short *yy_c_buf_p;
		unsigned short yy_hold_char;
		int yy_last_accepting_state;
		unsigned short *yy_last_accepting_cpos;
		int blockNesting;
		int yyleng;
		int yyTok;
		int yy_start;

	protected:
		CDFInterface *m_cdfInterface;
	};

	// defines units allowed for a certain property, used in parseUnit
	enum Units
	{
		FUnknown   = 0x0000,
		FInteger   = 0x0001,
		FNumber    = 0x0002,  // Real Numbers
		FPercent   = 0x0004,
		FLength    = 0x0008,
		FAngle     = 0x0010,
		FTime      = 0x0020,
		FFrequency = 0x0040,
		FRelative  = 0x0100,
		FNonNeg    = 0x0200
	};
};

#endif

// vim:ts=4:noet
