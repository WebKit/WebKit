/* Automatically generated using kdom/scripts/constants.pl. DO NOT EDIT ! */

#ifndef KDOM_DOMCONSTANTS_H
#define KDOM_DOMCONSTANTS_H

namespace KDOM
{
	struct DOMConfigurationConstants
	{
		typedef enum
		{
			ParameterNames, SetParameter, GetParameter, CanSetParameter 
		} DOMConfigurationConstantsEnum;
	};

	struct DOMStringListConstants
	{
		typedef enum
		{
			Length, Item, Contains 
		} DOMStringListConstantsEnum;
	};

	struct DOMLocatorConstants
	{
		typedef enum
		{
			LineNumber, ColumnNumber, ByteOffset, Utf16Offset, RelatedNode, 
			Uri 
		} DOMLocatorConstantsEnum;
	};

	struct DOMUserDataConstants
	{
		typedef enum
		{
			Dummy 
		} DOMUserDataConstantsEnum;
	};

	struct EntityConstants
	{
		typedef enum
		{
			PublicId, SystemId, NotationName, InputEncoding, XmlEncoding, 
			XmlVersion 
		} EntityConstantsEnum;
	};

	struct TextConstants
	{
		typedef enum
		{
			IsElementContentWhitespace, WholeText, SplitText, ReplaceWholeText 
		} TextConstantsEnum;
	};

	struct NodeConstants
	{
		typedef enum
		{
			NodeName, NodeValue, NodeType, ParentNode, ChildNodes, 
			FirstChild, LastChild, PreviousSibling, NextSibling, Attributes, OwnerDocument, 
			NamespaceURI, Prefix, LocalName, TextContent, BaseURI, InsertBefore, 
			ReplaceChild, RemoveChild, AppendChild, HasChildNodes, CloneNode, Normalize, 
			IsSupported, HasAttributes, CompareDocumentPosition, IsDefaultNamespace, LookupNamespaceURI, LookupPrefix, 
			IsEqualNode, IsSameNode, GetFeature 
		} NodeConstantsEnum;
	};

	struct ProcessingInstructionConstants
	{
		typedef enum
		{
			Target, Data 
		} ProcessingInstructionConstantsEnum;
	};

	struct DocumentConstants
	{
		typedef enum
		{
			Doctype, Implementation, DocumentElement, InputEncoding, XmlEncoding, 
			XmlStandalone, XmlVersion, StrictErrorChecking, DocumentURI, DomConfig, CreateElement, 
			CreateDocumentFragment, CreateTextNode, CreateComment, CreateCDATASection, CreateProcessingInstruction, CreateAttribute, 
			CreateEntityReference, GetElementsByTagName, ImportNode, NormalizeDocument, CreateElementNS, CreateAttributeNS, 
			GetElementsByTagNameNS, GetElementById, AdoptNode, RenameNode 
		} DocumentConstantsEnum;
	};

	struct NotationConstants
	{
		typedef enum
		{
			PublicId, SystemId 
		} NotationConstantsEnum;
	};

	struct DocumentTypeConstants
	{
		typedef enum
		{
			Name, Entities, Notations, PublicId, SystemId, 
			InternalSubset 
		} DocumentTypeConstantsEnum;
	};

	struct NodeListConstants
	{
		typedef enum
		{
			Length, Item 
		} NodeListConstantsEnum;
	};

	struct DOMImplementationConstants
	{
		typedef enum
		{
			Dummy, HasFeature, CreateDocumentType, CreateDocument, GetFeature, 
			CreateCSSStyleSheet, CreateLSParser, CreateLSSerializer, CreateLSInput, CreateLSOutput 
		} DOMImplementationConstantsEnum;
	};

	struct DOMExceptionConstants
	{
		typedef enum
		{
			Code 
		} DOMExceptionConstantsEnum;
	};

	struct AttrConstants
	{
		typedef enum
		{
			Name, Specified, Value, OwnerElement, IsId, 
			SchemaTypeInfo 
		} AttrConstantsEnum;
	};

	struct ElementConstants
	{
		typedef enum
		{
			TagName, SchemaTypeInfo, GetAttribute, SetAttribute, RemoveAttribute, 
			GetAttributeNode, SetAttributeNode, RemoveAttributeNode, GetElementsByTagName, GetAttributeNS, SetAttributeNS, 
			RemoveAttributeNS, GetAttributeNodeNS, SetAttributeNodeNS, GetElementsByTagNameNS, HasAttribute, HasAttributeNS, 
			SetIdAttribute, SetIdAttributeNS, SetIdAttributeNode 
		} ElementConstantsEnum;
	};

	struct CharacterDataConstants
	{
		typedef enum
		{
			Data, Length, SetData, SubstringData, AppendData, 
			InsertData, DeleteData, ReplaceData 
		} CharacterDataConstantsEnum;
	};

	struct NamedNodeMapConstants
	{
		typedef enum
		{
			Length, GetNamedItem, SetNamedItem, RemoveNamedItem, Item, 
			GetNamedItemNS, SetNamedItemNS, RemoveNamedItemNS 
		} NamedNodeMapConstantsEnum;
	};

	struct DOMErrorConstants
	{
		typedef enum
		{
			Severity, Message, Type, RelatedException, RelatedData, 
			Location 
		} DOMErrorConstantsEnum;
	};

	struct TypeInfoConstants
	{
		typedef enum
		{
			TypeName, TypeNamespace, IsDerivedFrom 
		} TypeInfoConstantsEnum;
	};

	struct DOMErrorHandlerConstants
	{
		typedef enum
		{
			Dummy, HandleError 
		} DOMErrorHandlerConstantsEnum;
	};
};

#endif
