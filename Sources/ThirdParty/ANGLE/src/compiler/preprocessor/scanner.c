/****************************************************************************\
Copyright (c) 2002, NVIDIA Corporation.

NVIDIA Corporation("NVIDIA") supplies this software to you in
consideration of your agreement to the following terms, and your use,
installation, modification or redistribution of this NVIDIA software
constitutes acceptance of these terms.  If you do not agree with these
terms, please do not use, install, modify or redistribute this NVIDIA
software.

In consideration of your agreement to abide by the following terms, and
subject to these terms, NVIDIA grants you a personal, non-exclusive
license, under NVIDIA's copyrights in this original NVIDIA software (the
"NVIDIA Software"), to use, reproduce, modify and redistribute the
NVIDIA Software, with or without modifications, in source and/or binary
forms; provided that if you redistribute the NVIDIA Software, you must
retain the copyright notice of NVIDIA, this notice and the following
text and disclaimers in all such redistributions of the NVIDIA Software.
Neither the name, trademarks, service marks nor logos of NVIDIA
Corporation may be used to endorse or promote products derived from the
NVIDIA Software without specific prior written permission from NVIDIA.
Except as expressly stated in this notice, no other rights or licenses
express or implied, are granted by NVIDIA herein, including but not
limited to any patent rights that may be infringed by your derivative
works or by other works in which the NVIDIA Software may be
incorporated. No hardware is licensed hereunder. 

THE NVIDIA SOFTWARE IS BEING PROVIDED ON AN "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED,
INCLUDING WITHOUT LIMITATION, WARRANTIES OR CONDITIONS OF TITLE,
NON-INFRINGEMENT, MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
ITS USE AND OPERATION EITHER ALONE OR IN COMBINATION WITH OTHER
PRODUCTS.

IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT,
INCIDENTAL, EXEMPLARY, CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, LOST PROFITS; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) OR ARISING IN ANY WAY
OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE
NVIDIA SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT,
TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF
NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\****************************************************************************/
//
// scanner.c
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if 0
    #include <ieeefp.h>
    #else
    #define isinff(x) (((*(int *)&(x) & 0x7f800000L)==0x7f800000L) && \
                       ((*(int *)&(x) & 0x007fffffL)==0000000000L))
#endif

#include "compiler/preprocessor/slglobals.h"


typedef struct StringInputSrc {
    InputSrc base;
    char *p;
} StringInputSrc;

static int eof_scan(InputSrc *is, yystypepp * yylvalpp)
{
    return EOF;
} // eof_scan

static void noop(InputSrc *in, int ch, yystypepp * yylvalpp) {}

static InputSrc eof_inputsrc = { 0, &eof_scan, &eof_scan, &noop };

static int byte_scan(InputSrc *, yystypepp * yylvalpp);

#define EOL_SY '\n'

#if defined(_MSC_VER)
    #define DBG_BREAKPOINT() __asm int 3
#elif defined(_M_AMD64)
    #define DBG_BREAKPOINT() assert(!"Dbg_Breakpoint");
#else
    #define DBG_BREAKPOINT()
#endif

#if defined(_MSC_VER) && !defined(_M_AMD64)
    __int64 RDTSC ( void ) {

        __int64 v;
    
        __asm __emit 0x0f
        __asm __emit 0x31
        __asm mov dword ptr v, eax
        __asm mov dword ptr v+4, edx
    
        return v;
    }
#endif


int InitScanner(CPPStruct *cpp)
{
    // Add various atoms needed by the CPP line scanner:
    if (!InitCPP())
        return 0;

    cpp->mostRecentToken = 0;
    cpp->tokenLoc = &cpp->ltokenLoc;

    cpp->ltokenLoc.file = 0;
    cpp->ltokenLoc.line = 0;

    cpp->currentInput = &eof_inputsrc;
    cpp->previous_token = '\n';
    cpp->pastFirstStatement = 0;

    return 1;
} // InitScanner

int FreeScanner(void)
{
    return (FreeCPP());
}

/*
 * str_getch()
 * takes care of reading from multiple strings.
 * returns the next-char from the input stream.
 * returns EOF when the complete shader is parsed.
 */
static int str_getch(StringInputSrc *in)
{
	for(;;){
	   if (*in->p){
	      if (*in->p == '\n') {
             in->base.line++;
             IncLineNumber();
          }
          return *in->p++;
	   }
	   if(++(cpp->PaWhichStr) < cpp->PaArgc){
		  free(in);
		  SetStringNumber(cpp->PaWhichStr);
    	  SetLineNumber(1);
		  ScanFromString(cpp->PaArgv[cpp->PaWhichStr]);
		  in=(StringInputSrc*)cpp->currentInput;
	      continue;             
	   }
	   else{
	      cpp->currentInput = in->base.prev;
	      cpp->PaWhichStr=0;
          free(in);
          return EOF;
       }  
	}
} // str_getch

static void str_ungetch(StringInputSrc *in, int ch, yystypepp *type) {
    if (in->p[-1] == ch)in->p--;
	else {
		*(in->p)='\0'; //this would take care of shifting to the previous string.
	    cpp->PaWhichStr--;
	}  
	if (ch == '\n') {
        in->base.line--;
        DecLineNumber();
    }
} // str_ungetch

int ScanFromString(char *s)
{
    
	StringInputSrc *in = malloc(sizeof(StringInputSrc));
    memset(in, 0, sizeof(StringInputSrc));
	in->p = s;
    in->base.line = 1;
    in->base.scan = byte_scan;
    in->base.getch = (int (*)(InputSrc *, yystypepp *))str_getch;
    in->base.ungetch = (void (*)(InputSrc *, int, yystypepp *))str_ungetch;
    in->base.prev = cpp->currentInput;
    cpp->currentInput = &in->base;

    return 1;
} // ScanFromString;


///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Floating point constants: /////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
/*
 * lBuildFloatValue() - Quick and dirty conversion to floating point.  Since all
 *         we need is single precision this should be quite precise.
 */

static float lBuildFloatValue(const char *str, int len, int exp)
{
    double val, expval, ten;
    int ii, llen, absexp;
    float rv;

    val = 0.0;
    llen = len;
    for (ii = 0; ii < len; ii++)
        val = val*10.0 + (str[ii] - '0');
    if (exp != 0) {
        absexp = exp > 0 ? exp : -exp;
        expval = 1.0f;
        ten = 10.0;
        while (absexp) {
            if (absexp & 1)
                expval *= ten;
            ten *= ten;
            absexp >>= 1;
        }
        if (exp >= 0) {
            val *= expval;
        } else {
            val /= expval;
        }
    }
    rv = (float)val;
    if (isinff(rv)) {
		CPPErrorToInfoLog(" ERROR___FP_CONST_OVERFLOW");
    }
    return rv;
} // lBuildFloatValue


/*
 * lFloatConst() - Scan a floating point constant.  Assumes that the scanner
 *         has seen at least one digit, followed by either a decimal '.' or the
 *         letter 'e'.
 */

static int lFloatConst(char *str, int len, int ch, yystypepp * yylvalpp)
{
    int HasDecimal, declen, exp, ExpSign;
    int str_len;
    float lval;
    
    HasDecimal = 0;
    declen = 0;
    exp = 0;
	
    str_len=len;
    if (ch == '.') {
		str[len++]=ch;
        HasDecimal = 1;
        ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
        while (ch >= '0' && ch <= '9') {
            if (len < MAX_SYMBOL_NAME_LEN) {
                declen++;
                if (len > 0 || ch != '0') {
                    str[len] = ch;
                    len++;str_len++;
                }
                ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            } else {
                CPPErrorToInfoLog("ERROR___FP_CONST_TOO_LONG");
                len = 1,str_len=1;
            }
        }
    }

    // Exponent:

    if (ch == 'e' || ch == 'E') {
        ExpSign = 1;
		str[len++]=ch;
        ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
        if (ch == '+') {
            str[len++]=ch;  
			ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
        } else if (ch == '-') {
            ExpSign = -1;
			str[len++]=ch;
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
        }
        if (ch >= '0' && ch <= '9') {
            while (ch >= '0' && ch <= '9') {
                exp = exp*10 + ch - '0';
				str[len++]=ch;
                ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            }
        } else {
            CPPErrorToInfoLog("ERROR___ERROR_IN_EXPONENT");
        }
        exp *= ExpSign;
    }
      
    if (len == 0) {
        lval = 0.0f;
		strcpy(str,"0.0");
    } else {
        str[len]='\0';      
        lval = lBuildFloatValue(str, str_len, exp - declen);
    }
    // Suffix:
    
    yylvalpp->sc_fval = lval;
    strcpy(yylvalpp->symbol_name,str);
    cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);            
    return CPP_FLOATCONSTANT;
} // lFloatConst

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////// Normal Scanner //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
    
static int byte_scan(InputSrc *in, yystypepp * yylvalpp)
{
    char symbol_name[MAX_SYMBOL_NAME_LEN + 1];
    char string_val[MAX_STRING_LEN + 1];
    int AlreadyComplained;
    int len, ch, ii, ival = 0;

    for (;;) {
        yylvalpp->sc_int = 0;
        ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
		
        while (ch == ' ' || ch == '\t' || ch == '\r') {
            yylvalpp->sc_int = 1;
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
        }
		
        cpp->ltokenLoc.file = cpp->currentInput->name;
        cpp->ltokenLoc.line = cpp->currentInput->line;
        len = 0;
        switch (ch) {
        default:
			return ch; // Single character token
        case EOF:
            return -1;
		case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y':
        case 'Z': case '_':
        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y':
        case 'z':            
            do {
                if (len < MAX_SYMBOL_NAME_LEN) {
                    symbol_name[len] = ch;
                    len++;
                    ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);					
                } else {
                    ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                }
            } while ((ch >= 'a' && ch <= 'z') ||
                     (ch >= 'A' && ch <= 'Z') ||
                     (ch >= '0' && ch <= '9') ||
                     ch == '_');
            if (len >= MAX_SYMBOL_NAME_LEN)
                len = MAX_SYMBOL_NAME_LEN - 1;
            symbol_name[len] = '\0';
            cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
            yylvalpp->sc_ident = LookUpAddString(atable, symbol_name);
            return CPP_IDENTIFIER;
            break;
        case '0':
            yylvalpp->symbol_name[len++] = ch;
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == 'x' || ch == 'X') {
				yylvalpp->symbol_name[len++] = ch;
                ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                if ((ch >= '0' && ch <= '9') ||
                    (ch >= 'A' && ch <= 'F') ||
                    (ch >= 'a' && ch <= 'f'))
                {
                    AlreadyComplained = 0;
                    ival = 0;
                    do {
						yylvalpp->symbol_name[len++] = ch;
                        if (ival <= 0x0fffffff) {
                            if (ch >= '0' && ch <= '9') {
                                ii = ch - '0';
                            } else if (ch >= 'A' && ch <= 'F') {
                                ii = ch - 'A' + 10;
                            } else {
                                ii = ch - 'a' + 10;
                            }
                            ival = (ival << 4) | ii;
                        } else {
                            if (!AlreadyComplained)
                                CPPErrorToInfoLog("ERROR___HEX_CONST_OVERFLOW");
                            AlreadyComplained = 1;
                        }
                        ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                    } while ((ch >= '0' && ch <= '9') ||
                             (ch >= 'A' && ch <= 'F') ||
                             (ch >= 'a' && ch <= 'f'));
                } else {
                    CPPErrorToInfoLog("ERROR___ERROR_IN_HEX_CONSTANT");
                }
                yylvalpp->symbol_name[len] = '\0';
				cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
				yylvalpp->sc_int = ival;
                return CPP_INTCONSTANT;
            } else if (ch >= '0' && ch <= '7') { // octal integer constants
                AlreadyComplained = 0;
                ival = 0;
                do {
                    yylvalpp->symbol_name[len++] = ch;
                    if (ival <= 0x1fffffff) {
                        ii = ch - '0';
                        ival = (ival << 3) | ii;
                    } else {
                        if (!AlreadyComplained)
                           CPPErrorToInfoLog("ERROR___OCT_CONST_OVERFLOW");
                        AlreadyComplained = 1;
                    }
                    ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                } while (ch >= '0' && ch <= '7');
                if (ch == '.' || ch == 'e' || ch == 'f' || ch == 'h' || ch == 'x'|| ch == 'E') 
                     return lFloatConst(yylvalpp->symbol_name, len, ch, yylvalpp);
                yylvalpp->symbol_name[len] = '\0';
				cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
				yylvalpp->sc_int = ival;
                return CPP_INTCONSTANT;
            } else {
				cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
				ch = '0';
            }
            // Fall through...
        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            do {
                if (len < MAX_SYMBOL_NAME_LEN) {
                    if (len > 0 || ch != '0') {
                        yylvalpp->symbol_name[len] = ch;
                   len++;
                    }
                    ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                }
            } while (ch >= '0' && ch <= '9');
            if (ch == '.' || ch == 'e' || ch == 'f' || ch == 'h' || ch == 'x'|| ch == 'E') {
                return lFloatConst(yylvalpp->symbol_name, len, ch, yylvalpp);
            } else {
                yylvalpp->symbol_name[len] = '\0';
				cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                ival = 0;
                AlreadyComplained = 0;
                for (ii = 0; ii < len; ii++) {
                    ch = yylvalpp->symbol_name[ii] - '0';
                    if ((ival > 214748364) || (ival == 214748364 && ch >= 8)) {
                        if (!AlreadyComplained)
                           CPPErrorToInfoLog("ERROR___INTEGER_CONST_OVERFLOW");
                        AlreadyComplained = 1;
                    }
                    ival = ival*10 + ch;
                }
                yylvalpp->sc_int = ival;
                if(ival==0)
                   strcpy(yylvalpp->symbol_name,"0");
                return CPP_INTCONSTANT;
            }
            break;
        case '-':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '-') {
                return CPP_DEC_OP;
            } else if (ch == '=') {
                return CPP_SUB_ASSIGN;
            } else {
                cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                return '-';
            }
        case '+':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '+') {
                return CPP_INC_OP;
            } else if (ch == '=') {
                return CPP_ADD_ASSIGN;
            } else {
                cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                return '+';
            }
        case '*':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '=') {
                return CPP_MUL_ASSIGN;
            } else {
                cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                return '*';
            }
        case '%':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '=') {
                return CPP_MOD_ASSIGN;
            } else if (ch == '>'){
                return CPP_RIGHT_BRACE;
            } else {
                cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                return '%';
            }
        case ':':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '>') {
                return CPP_RIGHT_BRACKET;
            } else {
                cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                return ':';
            }
        case '^':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '^') {
                return CPP_XOR_OP;
            } else {
                if (ch == '=')
                    return CPP_XOR_ASSIGN;
                else{
                  cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                  return '^';
                }
            }
        
        case '=':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '=') {
                return CPP_EQ_OP;
            } else {
                cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                return '=';
            }
        case '!':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '=') {
                return CPP_NE_OP;
            } else {
                cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                return '!';
            }
        case '|':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '|') {
                return CPP_OR_OP;
            } else {
                if (ch == '=')
                    return CPP_OR_ASSIGN;
                else{
                  cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                  return '|';
                }
            }
        case '&':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '&') {
                return CPP_AND_OP;
            } else {
                if (ch == '=')
                    return CPP_AND_ASSIGN;
                else{
                  cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                  return '&';
                }
            }
        case '<':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '<') {
                ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                if(ch == '=')
                    return CPP_LEFT_ASSIGN;
                else{
                    cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                    return CPP_LEFT_OP;
                }
            } else {
                if (ch == '=') {
                    return CPP_LE_OP;
                } else {
                    if (ch == '%')
                        return CPP_LEFT_BRACE;
                    else if (ch == ':')
                        return CPP_LEFT_BRACKET;
                    else{
                        cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                        return '<';
                    }
                }
            }
        case '>':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '>') {
                ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                if(ch == '=')
                    return CPP_RIGHT_ASSIGN;
                else{
                    cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                    return CPP_RIGHT_OP;
                }
            } else {
                if (ch == '=') {
                    return CPP_GE_OP;
                } else {
                    cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                    return '>';
                }
            }
        case '.':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch >= '0' && ch <= '9') {
                cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                return lFloatConst(yylvalpp->symbol_name, 0, '.', yylvalpp);
            } else {
                if (ch == '.') {
                    return -1; // Special EOF hack
                } else {
                    cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                    return '.';
                }
            }
        case '/':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            if (ch == '/') {
                do {
                    ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                } while (ch != '\n' && ch != EOF);
                if (ch == EOF)
                    return -1;
                return '\n';
            } else if (ch == '*') {
                int nlcount = 0;
                ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                do {
                    while (ch != '*') {
                        if (ch == '\n') nlcount++;
                        if (ch == EOF) {
                            CPPErrorToInfoLog("ERROR___EOF_IN_COMMENT");
                            return -1;
                        }
                        ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                    }
                    ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                    if (ch == EOF) {
                        CPPErrorToInfoLog("ERROR___EOF_IN_COMMENT");
                        return -1;
                    }
                } while (ch != '/');
                if (nlcount) {
                    return '\n';
                }
                // Go try it again...
            } else if (ch == '=') {
                return CPP_DIV_ASSIGN;
            } else {
                cpp->currentInput->ungetch(cpp->currentInput, ch, yylvalpp);
                return '/';
            }
            break;
        case '"':
            ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
            while (ch != '"' && ch != '\n' && ch != EOF) {
                if (ch == '\\') {
                    CPPErrorToInfoLog("The line continuation character (\\) is not part of the OpenGL ES Shading Language");
                    return -1;
                }
                if (len < MAX_STRING_LEN) {
                    string_val[len] = ch;
                    len++;
                    ch = cpp->currentInput->getch(cpp->currentInput, yylvalpp);
                }
            };
            string_val[len] = '\0';
            if (ch == '"') {
                yylvalpp->sc_ident = LookUpAddString(atable, string_val);
                return CPP_STRCONSTANT;
            } else {
                CPPErrorToInfoLog("ERROR___CPP_EOL_IN_STRING");
                return ERROR_SY;
            }
        }
    }
} // byte_scan

int yylex_CPP(char* buf, int maxSize)
{    
	yystypepp yylvalpp;
    int token = '\n';   

    for(;;) {

        char* tokenString = 0;
        token = cpp->currentInput->scan(cpp->currentInput, &yylvalpp);
		if(check_EOF(token))
		    return 0;
        if (token == '#') {
            if (cpp->previous_token == '\n'|| cpp->previous_token == 0) {
			    token = readCPPline(&yylvalpp);
                if(check_EOF(token))
                    return 0;
			    continue;
            } else {
                CPPErrorToInfoLog("preprocessor command must not be preceded by any other statement in that line");
                return 0;
            }
        }
        cpp->previous_token = token;
        // expand macros
        if (token == CPP_IDENTIFIER && MacroExpand(yylvalpp.sc_ident, &yylvalpp)) {
            cpp->pastFirstStatement = 1;
            continue;
        }
        
        if (token == '\n')
            continue;
          
        if (token == CPP_IDENTIFIER) {                
            cpp->pastFirstStatement = 1;
            tokenString = GetStringOfAtom(atable,yylvalpp.sc_ident);
        } else if (token == CPP_FLOATCONSTANT||token == CPP_INTCONSTANT){             
            cpp->pastFirstStatement = 1;            
            tokenString = yylvalpp.symbol_name;
		} else {            
            cpp->pastFirstStatement = 1;            
            tokenString = GetStringOfAtom(atable,token);
	    }

        if (tokenString) {
            if ((signed)strlen(tokenString) >= maxSize) {
                cpp->tokensBeforeEOF = 1;
                return maxSize;               
            } else  if (strlen(tokenString) > 0) {
			    strcpy(buf, tokenString);
                cpp->tokensBeforeEOF = 1;
                return (int)strlen(tokenString);
            }  

            return 0;
        }
    }

    return 0;
} // yylex

//Checks if the token just read is EOF or not.
int check_EOF(int token)
{
   if(token==-1){
       if(cpp->ifdepth >0){
		CPPErrorToInfoLog("#endif missing!! Compilation stopped");
        cpp->CompileError=1;
       }
      return 1;
   }
   return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// End of scanner.c //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

