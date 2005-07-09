/* Automatically generated using kdom/scripts/constants.pl. DO NOT EDIT ! */

#ifndef KDOM_LSCONSTANTS_H
#define KDOM_LSCONSTANTS_H

namespace KDOM
{
	struct LSExceptionConstants
	{
		typedef enum
		{
			Code 
		} LSExceptionConstantsEnum;
	};

	struct LSParserConstants
	{
		typedef enum
		{
			DomConfig, Filter, Async, Busy, Parse, 
			ParseURI, ParseWithContext, Abort 
		} LSParserConstantsEnum;
	};

	struct LSInputConstants
	{
		typedef enum
		{
			CharacterStream, ByteStream, StringData, SystemId, PublicId, 
			BaseURI, Encoding, CertifiedText 
		} LSInputConstantsEnum;
	};

	struct LSOutputConstants
	{
		typedef enum
		{
			SystemId, Encoding 
		} LSOutputConstantsEnum;
	};

	struct LSParserFilterConstants
	{
		typedef enum
		{
			WhatToShow, StartElement, AcceptNode 
		} LSParserFilterConstantsEnum;
	};

	struct LSSerializerConstants
	{
		typedef enum
		{
			DomConfig, NewLine, Filter, Write, WriteToURI, 
			WriteToString 
		} LSSerializerConstantsEnum;
	};

	struct LSSerializerFilterConstants
	{
		typedef enum
		{
			WhatToShow 
		} LSSerializerFilterConstantsEnum;
	};

	struct LSResourceResolverConstants
	{
		typedef enum
		{
			Dummy, ResolveResource 
		} LSResourceResolverConstantsEnum;
	};
};

#endif
