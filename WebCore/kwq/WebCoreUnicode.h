/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum
    {
        NoCategory,

        Mark_NonSpacing,          //   Mn
        Mark_SpacingCombining,    //   Mc
        Mark_Enclosing,           //   Me

        Number_DecimalDigit,      //   Nd
        Number_Letter,            //   Nl
        Number_Other,             //   No

        Separator_Space,          //   Zs
        Separator_Line,           //   Zl
        Separator_Paragraph,      //   Zp

        Other_Control,            //   Cc
        Other_Format,             //   Cf
        Other_Surrogate,          //   Cs
        Other_PrivateUse,         //   Co
        Other_NotAssigned,        //   Cn

        Letter_Uppercase,         //   Lu
        Letter_Lowercase,         //   Ll
        Letter_Titlecase,         //   Lt
        Letter_Modifier,          //   Lm
        Letter_Other,             //   Lo

        Punctuation_Connector,    //   Pc
        Punctuation_Dash,         //   Pd
        Punctuation_Open,         //   Ps
        Punctuation_Close,        //   Pe
        Punctuation_InitialQuote, //   Pi
        Punctuation_FinalQuote,   //   Pf
        Punctuation_Other,        //   Po

        Symbol_Math,              //   Sm
        Symbol_Currency,          //   Sc
        Symbol_Modifier,          //   Sk
        Symbol_Other              //   So
    } WebCoreUnicodeCategory;

    typedef enum
    {
        DirectionL, 	// Left Letter 
        DirectionR,	// Right Letter
        DirectionEN,	// European Number
        DirectionES,	// European Separator
        DirectionET,	// European Terminator (post/prefix e.g. $ and %)
        DirectionAN,	// Arabic Number
        DirectionCS,	// Common Separator 
        DirectionB, 	// Paragraph Separator (aka as PS)
        DirectionS, 	// Segment Separator (TAB)
        DirectionWS, 	// White space
        DirectionON,	// Other Neutral

	// types for explicit controls
        DirectionLRE, 
        DirectionLRO, 

        DirectionAL, 	// Arabic Letter (Right-to-left)

        DirectionRLE, 
        DirectionRLO, 
        DirectionPDF, 

        DirectionNSM, 	// Non-spacing Mark
        DirectionBN	// Boundary neutral (type of RLE etc after explicit levels)
    } WebCoreUnicodeDirection;

    typedef enum 
    {
        DecompositionSingle,
        DecompositionCanonical,
        DecompositionFont,
        DecompositionNoBreak,
        DecompositionInitial,
        DecompositionMedial,
        DecompositionFinal,
        DecompositionIsolated,
        DecompositionCircle,
        DecompositionSuper,
        DecompositionSub,
        DecompositionVertical,
        DecompositionWide,
        DecompositionNarrow,
        DecompositionSmall,
        DecompositionSquare,
        DecompositionCompat,
        DecompositionFraction
    } WebCoreUnicodeDecomposition;

    typedef enum
    {
        JoiningOther,
        JoiningDual,
        JoiningRight,
        JoiningCausing
    } WebCoreUnicodeJoining;

    typedef enum
    {
        Combining_BelowLeftAttached       = 200,
        Combining_BelowAttached           = 202,
        Combining_BelowRightAttached      = 204,
        Combining_LeftAttached            = 208,
        Combining_RightAttached           = 210,
        Combining_AboveLeftAttached       = 212,
        Combining_AboveAttached           = 214,
        Combining_AboveRightAttached      = 216,

        Combining_BelowLeft               = 218,
        Combining_Below                   = 220,
        Combining_BelowRight              = 222,
        Combining_Left                    = 224,
        Combining_Right                   = 226,
        Combining_AboveLeft               = 228,
        Combining_Above                   = 230,
        Combining_AboveRight              = 232,

        Combining_DoubleBelow             = 233,
        Combining_DoubleAbove             = 234,
        Combining_IotaSubscript           = 240
    } WebCoreUnicodeCombiningClass;

    extern int (*WebCoreUnicodeDigitValueFunction)(UniChar c);
    extern WebCoreUnicodeCategory (*WebCoreUnicodeCategoryFunction)(UniChar c);
    extern WebCoreUnicodeDirection (*WebCoreUnicodeDirectionFunction)(UniChar c);
    extern WebCoreUnicodeJoining (*WebCoreUnicodeJoiningFunction)(UniChar c);
    extern WebCoreUnicodeDecomposition (*WebCoreUnicodeDecompositionTagFunction)(UniChar c);
    extern bool (*WebCoreUnicodeMirroredFunction)(UniChar c);
    extern UniChar (*WebCoreUnicodeMirroredCharFunction)(UniChar c);
    extern WebCoreUnicodeCombiningClass (*WebCoreUnicodeCombiningClassFunction)(UniChar c);
    extern UniChar (*WebCoreUnicodeLowerFunction)(UniChar c);
    extern UniChar (*WebCoreUnicodeUpperFunction)(UniChar c);

#ifdef __cplusplus
}
#endif