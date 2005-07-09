/* Automatically generated using kdom/scripts/constants.pl. DO NOT EDIT ! */

#ifndef KDOM_TRAVERSALCONSTANTS_H
#define KDOM_TRAVERSALCONSTANTS_H

namespace KDOM
{
	struct DocumentTraversalConstants
	{
		typedef enum
		{
			Dummy, CreateNodeIterator, CreateTreeWalker 
		} DocumentTraversalConstantsEnum;
	};

	struct NodeIteratorConstants
	{
		typedef enum
		{
			Root, WhatToShow, Filter, ExpandEntityReferences, NextNode, 
			PreviousNode, Detach 
		} NodeIteratorConstantsEnum;
	};

	struct TreeWalkerConstants
	{
		typedef enum
		{
			Root, WhatToShow, Filter, ExpandEntityReferences, CurrentNode, 
			ParentNode, FirstChild, LastChild, PreviousSibling, NextSibling, PreviousNode, 
			NextNode 
		} TreeWalkerConstantsEnum;
	};
};

#endif
