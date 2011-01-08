/*
**********************************************************************
*   Copyright (C) 1997-2004, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* File USCRIPT.H
*
* Modification History:
*
*   Date        Name        Description
*   07/06/2001    Ram         Creation.
******************************************************************************
*/
#ifndef USCRIPT_H
#define USCRIPT_H
#include "unicode/utypes.h"

/**
 * Constants for Unicode script values from ScriptNames.txt .
 *
 * @stable ICU 2.2
 */
typedef enum UScriptCode {
      USCRIPT_INVALID_CODE = -1,
      USCRIPT_COMMON       =  0 , /* Zyyy */
      USCRIPT_INHERITED    =  1,  /* Qaai */
      USCRIPT_ARABIC       =  2,  /* Arab */
      USCRIPT_ARMENIAN     =  3,  /* Armn */
      USCRIPT_BENGALI      =  4,  /* Beng */
      USCRIPT_BOPOMOFO     =  5,  /* Bopo */
      USCRIPT_CHEROKEE     =  6,  /* Cher */
      USCRIPT_COPTIC       =  7,  /* Copt */
      USCRIPT_CYRILLIC     =  8,  /* Cyrl (Cyrs) */
      USCRIPT_DESERET      =  9,  /* Dsrt */
      USCRIPT_DEVANAGARI   = 10,  /* Deva */
      USCRIPT_ETHIOPIC     = 11,  /* Ethi */
      USCRIPT_GEORGIAN     = 12,  /* Geor (Geon, Geoa) */
      USCRIPT_GOTHIC       = 13,  /* Goth */
      USCRIPT_GREEK        = 14,  /* Grek */
      USCRIPT_GUJARATI     = 15,  /* Gujr */
      USCRIPT_GURMUKHI     = 16,  /* Guru */
      USCRIPT_HAN          = 17,  /* Hani */
      USCRIPT_HANGUL       = 18,  /* Hang */
      USCRIPT_HEBREW       = 19,  /* Hebr */
      USCRIPT_HIRAGANA     = 20,  /* Hira */
      USCRIPT_KANNADA      = 21,  /* Knda */
      USCRIPT_KATAKANA     = 22,  /* Kana */
      USCRIPT_KHMER        = 23,  /* Khmr */
      USCRIPT_LAO          = 24,  /* Laoo */
      USCRIPT_LATIN        = 25,  /* Latn (Latf, Latg) */
      USCRIPT_MALAYALAM    = 26,  /* Mlym */
      USCRIPT_MONGOLIAN    = 27,  /* Mong */
      USCRIPT_MYANMAR      = 28,  /* Mymr */
      USCRIPT_OGHAM        = 29,  /* Ogam */
      USCRIPT_OLD_ITALIC   = 30,  /* Ital */
      USCRIPT_ORIYA        = 31,  /* Orya */
      USCRIPT_RUNIC        = 32,  /* Runr */
      USCRIPT_SINHALA      = 33,  /* Sinh */
      USCRIPT_SYRIAC       = 34,  /* Syrc (Syrj, Syrn, Syre) */
      USCRIPT_TAMIL        = 35,  /* Taml */
      USCRIPT_TELUGU       = 36,  /* Telu */
      USCRIPT_THAANA       = 37,  /* Thaa */
      USCRIPT_THAI         = 38,  /* Thai */
      USCRIPT_TIBETAN      = 39,  /* Tibt */
      /** Canadian_Aboriginal script. @stable ICU 2.6 */
      USCRIPT_CANADIAN_ABORIGINAL = 40,  /* Cans */
      /** Canadian_Aboriginal script (alias). @stable ICU 2.2 */
      USCRIPT_UCAS         = USCRIPT_CANADIAN_ABORIGINAL,
      USCRIPT_YI           = 41,  /* Yiii */
      USCRIPT_TAGALOG      = 42,  /* Tglg */
      USCRIPT_HANUNOO      = 43,  /* Hano */
      USCRIPT_BUHID        = 44,  /* Buhd */
      USCRIPT_TAGBANWA     = 45,  /* Tagb */

      /* New scripts in Unicode 4 @stable ICU 2.6 */
      USCRIPT_BRAILLE,            /* Brai */
      USCRIPT_CYPRIOT,            /* Cprt */
      USCRIPT_LIMBU,              /* Limb */
      USCRIPT_LINEAR_B,           /* Linb */
      USCRIPT_OSMANYA,            /* Osma */
      USCRIPT_SHAVIAN,            /* Shaw */
      USCRIPT_TAI_LE,             /* Tale */
      USCRIPT_UGARITIC,           /* Ugar */

      /** New script code in Unicode 4.0.1 @draft ICU 3.0 */
      USCRIPT_KATAKANA_OR_HIRAGANA,/*Hrkt */

      USCRIPT_CODE_LIMIT
} UScriptCode;

/**
 * Gets script codes associated with the given locale or ISO 15924 abbreviation or name. 
 * Fills in USCRIPT_MALAYALAM given "Malayam" OR "Mlym".
 * Fills in USCRIPT_LATIN given "en" OR "en_US" 
 * If required capacity is greater than capacity of the destination buffer then the error code
 * is set to U_BUFFER_OVERFLOW_ERROR and the required capacity is returned
 *
 * <p>Note: To search by short or long script alias only, use
 * u_getPropertyValueEnum(UCHAR_SCRIPT, alias) instead.  This does
 * a fast lookup with no access of the locale data.
 * @param nameOrAbbrOrLocale name of the script, as given in
 * PropertyValueAliases.txt, or ISO 15924 code or locale
 * @param fillIn the UScriptCode buffer to fill in the script code
 * @param capacity the capacity (size) fo UScriptCode buffer passed in.
 * @param err the error status code.
 * @return The number of script codes filled in the buffer passed in 
 * @stable ICU 2.4
 */
U_STABLE int32_t  U_EXPORT2 
uscript_getCode(const char* nameOrAbbrOrLocale,UScriptCode* fillIn,int32_t capacity,UErrorCode *err);

/**
 * Gets a script name associated with the given script code. 
 * Returns  "Malayam" given USCRIPT_MALAYALAM
 * @param scriptCode UScriptCode enum
 * @return script long name as given in
 * PropertyValueAliases.txt, or NULL if scriptCode is invalid
 * @stable ICU 2.4
 */
U_STABLE const char*  U_EXPORT2 
uscript_getName(UScriptCode scriptCode);

/**
 * Gets a script name associated with the given script code. 
 * Returns  "Mlym" given USCRIPT_MALAYALAM
 * @param scriptCode UScriptCode enum
 * @return script abbreviated name as given in
 * PropertyValueAliases.txt, or NULL if scriptCode is invalid
 * @stable ICU 2.4
 */
U_STABLE const char*  U_EXPORT2 
uscript_getShortName(UScriptCode scriptCode);

/** 
 * Gets the script code associated with the given codepoint.
 * Returns USCRIPT_MALAYALAM given 0x0D02 
 * @param codepoint UChar32 codepoint
 * @param err the error status code.
 * @return The UScriptCode, or 0 if codepoint is invalid 
 * @stable ICU 2.4
 */
U_STABLE UScriptCode  U_EXPORT2 
uscript_getScript(UChar32 codepoint, UErrorCode *err);

#endif


