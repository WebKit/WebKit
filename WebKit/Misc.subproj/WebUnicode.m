/*	
        WebUnicode.m
	Copyright 2001, 2002, Apple Computer, Inc.
*/
#import <WebKit/WebUnicode.h>

#import <WebCore/WebCoreUnicode.h>

#define CELL(ucs) ((unsigned char) ucs & 0xff)
#define ROW(ucs) ((unsigned char) (ucs>>8)&0xff)


static int _unicodeDigitValue(UniChar c)
{
    const char *dec_row = decimal_info[ROW(c)];
    if( !dec_row )
	return -1;
    return dec_row[CELL(c)];
}

static WebCoreUnicodeCategory _unicodeCategory(UniChar c)
{
    return (WebCoreUnicodeCategory)(unicode_info[ROW(c)][CELL(c)]);
}

static WebCoreUnicodeDirection _unicodeDirection(UniChar c)
{
    const unsigned char *rowp = direction_info[ROW(c)];
    if(!rowp) 
        return DirectionL;
    return (WebCoreUnicodeDirection) ( *(rowp+CELL(c)) &0x1f );
}

static WebCoreUnicodeJoining _unicodeJoining(UniChar c)
{
    const unsigned char *rowp = direction_info[ROW(c)];
    if ( !rowp )
	return JoiningOther;
    return (WebCoreUnicodeJoining) ((*(rowp+CELL(c)) >> 5) &0x3);
}

static WebCoreUnicodeDecomposition _unicodeDecompositionTag(UniChar c)
{
    const unsigned short *r = decomposition_info[ROW(c)];
    if(!r)
        return DecompositionSingle;

    unsigned short pos = r[CELL(c)];
    if(!pos)
        return DecompositionSingle;

    return (WebCoreUnicodeDecomposition) decomposition_map[pos];
}

static bool _unicodeMirrored(UniChar c)
{
    const unsigned char *rowp = direction_info[ROW(c)];
    if ( !rowp )
	return FALSE;
    return *(rowp+CELL(c))>128;
}

static UniChar _unicodeMirroredChar(UniChar c)
{
    if(!_unicodeMirrored(c))
        return c;

    int i;
    for (i = 0; i < symmetricPairsSize; i ++) {
	if (symmetricPairs[i] == c)
	  return symmetricPairs[(i%2) ? (i-1) : (i+1)];
    }
    return c;
}

static WebCoreUnicodeCombiningClass _unicodeCombiningClass (UniChar c)
{
    const unsigned char *rowp = combining_info[ROW(c)];
    if ( !rowp )
	return 0;
    return *(rowp+CELL(c));
}

static UniChar _unicodeLower(UniChar c)
{
    if ( _unicodeCategory(c) != Letter_Uppercase )
	return c;
    unsigned short lower = *( case_info[ROW(c)] + CELL(c) );
    if ( lower == 0 )
	return c;
    return lower;
}

static UniChar _unicodeUpper(UniChar c)
{
    if ( _unicodeCategory(c) != Letter_Lowercase )
	return c;
    unsigned short upper = *(case_info[ROW(c)]+CELL(c));
    if ( upper == 0 )
	return c;
    return upper;
}

void WebKitInitializeUnicode(void)
{
    WebCoreUnicodeDigitValueFunction = _unicodeDigitValue;
    WebCoreUnicodeCategoryFunction = _unicodeCategory;
    WebCoreUnicodeDirectionFunction = _unicodeDirection;
    WebCoreUnicodeJoiningFunction = _unicodeJoining;
    WebCoreUnicodeDecompositionTagFunction = _unicodeDecompositionTag;
    WebCoreUnicodeMirroredFunction = _unicodeMirrored;
    WebCoreUnicodeMirroredCharFunction = _unicodeMirroredChar;
    WebCoreUnicodeCombiningClassFunction = _unicodeCombiningClass;
    WebCoreUnicodeLowerFunction = _unicodeLower;
    WebCoreUnicodeUpperFunction = _unicodeUpper;
}

#ifdef QT_CODE

// small class used internally in QString::Compose()
class QLigature
{
public:
    QLigature( QChar c );

    Q_UINT16 first() { cur = ligatures; return cur ? *cur : 0; }
    Q_UINT16 next() { return cur && *cur ? *(cur++) : 0; }
    Q_UINT16 current() { return cur ? *cur : 0; }

    int match(QString & str, unsigned int index);
    QChar head();
    QChar::Decomposition tag();

private:
    Q_UINT16 *ligatures;
    Q_UINT16 *cur;
};

QLigature::QLigature( QChar c )
{
    const Q_UINT16 *r = ligature_info[c.row()];
    if( !r )
	ligatures = 0;
    else
    {
	const Q_UINT16 pos = r[c.cell()];
	ligatures = (Q_UINT16 *)&(ligature_map[pos]);
    }
    cur = ligatures;
}

QChar QLigature::head()
{
    if(current())
	return QChar(decomposition_map[current()+1]);

    return QChar::null;
}

QChar::Decomposition QLigature::tag()
{
    if(current())
	return (QChar::Decomposition) decomposition_map[current()];

    return QChar::Canonical;
}

int QLigature::match(QString & str, unsigned int index)
{
    unsigned int i=index;

    if(!current()) return 0;

    Q_UINT16 lig = current() + 2;
    Q_UINT16 ch;

    while ((i < str.length()) && (ch = decomposition_map[lig])) {
	if (str[(int)i] != QChar(ch))
	    return 0;
	i++; lig++;
    }

    if (!decomposition_map[lig])
    {
	return i-index;
    }
    return 0;
}


// this function is just used in QString::compose()
static inline bool format(QChar::Decomposition tag, QString & str,
			  int index, int len)
{
    unsigned int l = index + len;
    unsigned int r = index;

    bool left = FALSE, right = FALSE;

    left = ((l < str.length()) &&
	    ((str[(int)l].joining() == QChar::Dual) ||
	     (str[(int)l].joining() == QChar::Right)));
    if (r > 0) {
	r--;
	//printf("joining(right) = %d\n", str[(int)r].joining());
	right = (str[(int)r].joining() == QChar::Dual);
    }


    switch (tag) {
    case QChar::Medial:
	return (left & right);
    case QChar::Initial:
	return (left && !right);
    case QChar::Final:
	return (right);// && !left);
    case QChar::Isolated:
    default:
	return (!right && !left);
    }
} // format()

/*
  QString::compose() and visual() were developed by Gordon Tisher
  <tisher@uniserve.ca>, with input from Lars Knoll <knoll@mpi-hd.mpg.de>,
  who developed the unicode data tables.
*/
/*!
    \warning This function is not supported in Qt 3.x. It is provided
    for experimental and illustrative purposes only. It is mainly of
    interest to those experimenting with Arabic and other
    composition-rich texts.

    Applies possible ligatures to a QString. Useful when
    composition-rich text requires rendering with glyph-poor fonts,
    but it also makes compositions such as QChar(0x0041) ('A') and
    QChar(0x0308) (Unicode accent diaresis), giving QChar(0x00c4)
    (German A Umlaut).
*/
void QString::compose()
{
#ifndef QT_NO_UNICODETABLES
    unsigned int index=0, len;
    unsigned int cindex = 0;

    QChar code, head;

    QMemArray<QChar> dia;

    QString composed = *this;

    while (index < length()) {
	code = at(index);
	//printf("\n\nligature for 0x%x:\n", code.unicode());
	QLigature ligature(code);
	ligature.first();
	while(ligature.current()) {
	    if ((len = ligature.match(*this, index)) != 0) {
		head = ligature.head();
		unsigned short code = head.unicode();
		// we exclude Arabic presentation forms A and a few
		// other ligatures, which are undefined in most fonts
		if(!(code > 0xfb50 && code < 0xfe80) &&
		   !(code > 0xfb00 && code < 0xfb2a)) {
				// joining info is only needed for Arabic
		    if (format(ligature.tag(), *this, index, len)) {
			//printf("using ligature 0x%x, len=%d\n",code,len);
			// replace letter
			composed.replace(cindex, len, QChar(head));
			index += len-1;
			// we continue searching in case we have a final
			// form because medial ones are preferred.
			if ( len != 1 || ligature.tag() !=QChar::Final )
			    break;
		    }
		}
	    }
	    ligature.next();
	}
	cindex++;
	index++;
    }
    *this = composed;
#endif
}






bool QChar::isPrint() const
{
    Category c = category();
    return !(c == Other_Control || c == Other_NotAssigned);
}

/*!
    Returns TRUE if the character is a separator character
    (Separator_* categories); otherwise returns FALSE.
*/
bool QChar::isSpace() const
{
    if( !row() )
	if( cell() >= 9 && cell() <=13 ) return TRUE;
    Category c = category();
    return c >= Separator_Space && c <= Separator_Paragraph;
}

/*!
    Returns TRUE if the character is a mark (Mark_* categories);
    otherwise returns FALSE.
*/
bool QChar::isMark() const
{
    Category c = category();
    return c >= Mark_NonSpacing && c <= Mark_Enclosing;
}

/*!
    Returns TRUE if the character is a punctuation mark (Punctuation_*
    categories); otherwise returns FALSE.
*/
bool QChar::isPunct() const
{
    Category c = category();
    return (c >= Punctuation_Connector && c <= Punctuation_Other);
}

/*!
    Returns TRUE if the character is a letter (Letter_* categories);
    otherwise returns FALSE.
*/
bool QChar::isLetter() const
{
    Category c = category();
    return (c >= Letter_Uppercase && c <= Letter_Other);
}

/*!
    Returns TRUE if the character is a number (of any sort - Number_*
    categories); otherwise returns FALSE.

    \sa isDigit()
*/
bool QChar::isNumber() const
{
    Category c = category();
    return c >= Number_DecimalDigit && c <= Number_Other;
}

/*!
    Returns TRUE if the character is a letter or number (Letter_* or
    Number_* categories); otherwise returns FALSE.
*/
bool QChar::isLetterOrNumber() const
{
    Category c = category();
    return (c >= Letter_Uppercase && c <= Letter_Other)
	|| (c >= Number_DecimalDigit && c <= Number_Other);
}


/*!
    Returns TRUE if the character is a decimal digit
    (Number_DecimalDigit); otherwise returns FALSE.
*/
bool QChar::isDigit() const
{
    return (category() == Number_DecimalDigit);
}


/*!
    Returns TRUE if the character is a symbol (Symbol_* categories);
    otherwise returns FALSE.
*/
bool QChar::isSymbol() const
{
    Category c = category();
    return c >= Symbol_Math && c <= Symbol_Other;
}
#endif
