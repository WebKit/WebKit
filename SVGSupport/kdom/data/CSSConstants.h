/* Automatically generated using kdom/scripts/constants.pl. DO NOT EDIT ! */

#ifndef KDOM_CSSCONSTANTS_H
#define KDOM_CSSCONSTANTS_H

namespace KDOM
{
	struct RectConstants
	{
		typedef enum
		{
			Top, Right, Bottom, Left 
		} RectConstantsEnum;
	};

	struct DocumentStyleConstants
	{
		typedef enum
		{
			StyleSheets 
		} DocumentStyleConstantsEnum;
	};

	struct CSSStyleSheetConstants
	{
		typedef enum
		{
			OwnerRule, CssRules, InsertRule, DeleteRule 
		} CSSStyleSheetConstantsEnum;
	};

	struct CSSImportRuleConstants
	{
		typedef enum
		{
			Href, Media, StyleSheet 
		} CSSImportRuleConstantsEnum;
	};

	struct CSSRuleListConstants
	{
		typedef enum
		{
			Length, Item 
		} CSSRuleListConstantsEnum;
	};

	struct CSSPageRuleConstants
	{
		typedef enum
		{
			SelectorText, Style 
		} CSSPageRuleConstantsEnum;
	};

	struct CSSMediaRuleConstants
	{
		typedef enum
		{
			Media, CssRules, InsertRule, DeleteRule 
		} CSSMediaRuleConstantsEnum;
	};

	struct CSSRuleConstants
	{
		typedef enum
		{
			Type, CssText, ParentStyleSheet, ParentRule 
		} CSSRuleConstantsEnum;
	};

	struct StyleSheetListConstants
	{
		typedef enum
		{
			Length, Item 
		} StyleSheetListConstantsEnum;
	};

	struct StyleSheetConstants
	{
		typedef enum
		{
			Type, Disabled, OwnerNode, ParentStyleSheet, Href, 
			Title, Media 
		} StyleSheetConstantsEnum;
	};

	struct CSSCharsetRuleConstants
	{
		typedef enum
		{
			Encoding 
		} CSSCharsetRuleConstantsEnum;
	};

	struct CSSPrimitiveValueConstants
	{
		typedef enum
		{
			PrimitiveType, SetFloatValue, GetFloatValue, SetStringValue, GetStringValue, 
			GetCounterValue, GetRectValue, GetRGBColorValue 
		} CSSPrimitiveValueConstantsEnum;
	};

	struct CSSStyleRuleConstants
	{
		typedef enum
		{
			SelectorText, Style 
		} CSSStyleRuleConstantsEnum;
	};

	struct RGBColorConstants
	{
		typedef enum
		{
			Red, Green, Blue 
		} RGBColorConstantsEnum;
	};

	struct MediaListConstants
	{
		typedef enum
		{
			MediaText, Length, Item, DeleteMedium, AppendMedium 
		} MediaListConstantsEnum;
	};

	struct CSSValueListConstants
	{
		typedef enum
		{
			Length, Item 
		} CSSValueListConstantsEnum;
	};

	struct CounterConstants
	{
		typedef enum
		{
			Identifier, ListStyle, Separator 
		} CounterConstantsEnum;
	};

	struct CSSStyleDeclarationConstants
	{
		typedef enum
		{
			CssText, Length, ParentRule, GetPropertyValue, GetPropertyCSSValue, 
			RemoveProperty, GetPropertyPriority, SetProperty, Item 
		} CSSStyleDeclarationConstantsEnum;
	};

	struct CSSValueConstants
	{
		typedef enum
		{
			CssText, CssValueType 
		} CSSValueConstantsEnum;
	};

	struct CSSFontFaceRuleConstants
	{
		typedef enum
		{
			Style 
		} CSSFontFaceRuleConstantsEnum;
	};
};

#endif
