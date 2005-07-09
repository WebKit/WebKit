/* Automatically generated using kdom/scripts/constants.pl. DO NOT EDIT ! */

#ifndef KDOM_RANGECONSTANTS_H
#define KDOM_RANGECONSTANTS_H

namespace KDOM
{
	struct RangeExceptionConstants
	{
		typedef enum
		{
			Code 
		} RangeExceptionConstantsEnum;
	};

	struct DocumentRangeConstants
	{
		typedef enum
		{
			Dummy, CreateRange 
		} DocumentRangeConstantsEnum;
	};

	struct RangeConstants
	{
		typedef enum
		{
			StartContainer, StartOffset, EndContainer, EndOffset, Collapsed, 
			CommonAncestorContainer, SetStart, SetEnd, SetStartAfter, SetStartBefore, SetEndAfter, 
			SetEndBefore, Collapse, SelectNode, SelectNodeContents, CompareBoundaryPoints, DeleteContents, 
			ExtractContents, CloneContents, InsertNode, SurroundContents, CloneRange, ToString, 
			Detach 
		} RangeConstantsEnum;
	};
};

#endif
