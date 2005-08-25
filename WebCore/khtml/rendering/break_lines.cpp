#include <break_lines.h>

#ifdef HAVE_THAI_BREAKS
#include "ThBreakIterator.h"

static const QChar *cached = 0;
static QCString *cachedString = 0;
static ThBreakIterator *thaiIt = 0;
#endif

#if APPLE_CHANGES
#include <CoreServices/CoreServices.h>
#endif

void cleanupLineBreaker()
{
#ifdef HAVE_THAI_BREAKS
    if ( cachedString )
	delete cachedString;
    if ( thaiIt )
	delete thaiIt;
    cached = 0;
#endif
}

namespace khtml {
/*
  This function returns true, if the string can bre broken before the 
  character at position pos in the string s with length len
*/
bool isBreakable(const QChar *s, int pos, int len, bool breakNBSP)
{
#if !APPLE_CHANGES
    const QChar *c = s+pos;
    unsigned short ch = c->unicode();
    if ( ch > 0xff ) {
	// not latin1, need to do more sophisticated checks for asian fonts
	unsigned char row = c->row();
	if ( row == 0x0e ) {
	    // 0e00 - 0e7f == Thai
	    if ( c->cell() < 0x80 ) {
#ifdef HAVE_THAI_BREAKS
		// check for thai
		if( s != cached ) {
		    // build up string of thai chars
		    QTextCodec *thaiCodec = QTextCodec::codecForMib(2259);
		    if ( !cachedString )
			cachedString = new QCString;
		    if ( !thaiIt )
			thaiIt = ThBreakIterator::createWordInstance(); 
		    *cachedString = thaiCodec->fromUnicode( QConstString( (QChar *)s, len ).qstring() );
		}
		thaiIt->setText((tchar *)cachedString->data());
		for(int i = thaiIt->first(); i != thaiIt->DONE; i = thaiIt->next() ) {
		    if( i == pos )
			return true;
		    if( i > pos )
			return false;
		}
		return false;
#else
		// if we don't have a thai line breaking lib, allow
		// breaks everywhere except directly before punctuation.
		return true;
#endif
	    } else 
		return false;
	}
	if ( row > 0x2d && row < 0xfb || row == 0x11 )
	    // asian line breaking. Everywhere allowed except directly
	    // in front of a punctuation character.
	    return true;
	else // no asian font
	    return c->isSpace();
    } else {
	if ( ch == ' ' || ch == '\t' || ch == '\n' )
	    return true;
    }
    return false;
#else
    OSStatus status = 0, findStatus = 0;
    static TextBreakLocatorRef breakLocator = 0;
    UniCharArrayOffset end;
    const QChar *c = s+pos;
    unsigned short ch = c->unicode();

    // Newlines and spaces are always breakable, as are non-breaking spaces when breakNBSP is true.
    // FIXME: Tiger is not allowing breaks on spaces after bullet characters like &#183;.  We don't know
    // at the moment whether this behavior is correct or not.  Since Tiger is also not allowing breaks on spaces
    // after hyphen-like characters, this does not seem ideal for the Web.  Therefore for now we override space
    // characters up front and bypass the Unicode line breaking routines.
    if (ch == ' ' || ch == '\n' || ch == '\t' || (breakNBSP && ch == 0xa0))
        return true;

    // If current character, or the previous character aren't simple latin1 then
    // use the UC line break locator.  UCFindTextBreak will report false if we
    // have a sequence of 0xa0 0x20 (nbsp, sp), so we explicity check for that
    // case.
    unsigned short lastCh = pos > 0 ? (s+pos-1)->unicode() : 0;
    if ((ch > 0x7f && ch != 0xa0) || (lastCh > 0x7f && lastCh != 0xa0)) {
        if (!breakLocator)
            status = UCCreateTextBreakLocator(NULL, 0, kUCTextBreakLineMask, &breakLocator);
        if (status == 0)
            findStatus = UCFindTextBreak(breakLocator, kUCTextBreakLineMask, 0, (const UniChar *)s, len, pos, &end);

        // If carbon fails, fail back on simple white space detection.
        if (findStatus == 0)
            return ((int)end == pos) ? true : false;
    }
    
    // Match WinIE's breaking strategy, which is to always allow breaks after hyphens and question marks.
    if (lastCh == '-' || lastCh == '?')
        return true;

    // No other ascii chars are breakable.
    return false;
#endif    
}

};
