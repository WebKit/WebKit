/*	
        WebUnicode.m
	Copyright 2001, 2002, Apple Computer, Inc.
*/
#import <WebKit/WebUnicode.h>
#import <WebCore/WebCoreUnicode.h>

#import <unicode/uchar.h>

static int _unicodeDigitValue(UChar32 c)
{
    return u_charDigitValue(c);
}

static WebCoreUnicodeDirection _unicodeDirection(UChar32 c)
{
    return u_charDirection(c);
}

static bool _unicodeMirrored(UChar32 c)
{
    return u_isMirrored(c);
}

static UChar32 _unicodeMirroredChar(UChar32 c)
{
    return u_charMirror(c);
}

static UChar32 _unicodeLower(UChar32 c)
{
    return u_tolower(c);
}

static UChar32 _unicodeUpper(UChar32 c)
{
    return u_toupper(c);
}

void WebKitInitializeUnicode(void)
{
    WebCoreUnicodeDigitValueFunction = _unicodeDigitValue;
    WebCoreUnicodeDirectionFunction = _unicodeDirection;
    WebCoreUnicodeMirroredFunction = _unicodeMirrored;
    WebCoreUnicodeMirroredCharFunction = _unicodeMirroredChar;
    WebCoreUnicodeLowerFunction = _unicodeLower;
    WebCoreUnicodeUpperFunction = _unicodeUpper;
}

bool hasShapeForNextCharacter (const CharacterShapeIterator *iterator)
{
    if (iterator->currentCharacter - iterator->run->characters >= iterator->run->to)
        return false;
    return true;
}

UniChar shapeForNextCharacter (CharacterShapeIterator *iterator)
{
    if (!hasShapeForNextCharacter(iterator))
        return 0;
    
    UniChar currentCharacter = *iterator->currentCharacter;
    iterator->currentCharacter++;

    return currentCharacter;
}

// Assumes input characters are logically ordered.
bool initializeCharacterShapeIterator (CharacterShapeIterator *iterator, const WebCoreTextRun *run)
{        
    iterator->currentCharacter = run->characters + run->from;
    iterator->run = run;
    
    return true;
}
