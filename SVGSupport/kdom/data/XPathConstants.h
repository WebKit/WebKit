/* Automatically generated using kdom/scripts/constants.pl. DO NOT EDIT ! */

#ifndef KDOM_XPATHCONSTANTS_H
#define KDOM_XPATHCONSTANTS_H

namespace KDOM
{
	struct XPathEvaluatorConstants
	{
		typedef enum
		{
			Dummy, CreateExpression, CreateNSResolver, Evaluate 
		} XPathEvaluatorConstantsEnum;
	};

	struct XPathNSResolverConstants
	{
		typedef enum
		{
			Dummy, LookupNamespaceURI 
		} XPathNSResolverConstantsEnum;
	};

	struct XPathNamespaceConstants
	{
		typedef enum
		{
			Dummy 
		} XPathNamespaceConstantsEnum;
	};

	struct XPathExceptionConstants
	{
		typedef enum
		{
			Code 
		} XPathExceptionConstantsEnum;
	};

	struct XPathExpressionConstants
	{
		typedef enum
		{
			Dummy, Evaluate 
		} XPathExpressionConstantsEnum;
	};

	struct XPathResultConstants
	{
		typedef enum
		{
			ResultType, NumberValue, StringValue, BooleanValue, SingleNodeValue, 
			InvalidIteratorState, IterateNext, SnapshotItem 
		} XPathResultConstantsEnum;
	};
};

#endif
