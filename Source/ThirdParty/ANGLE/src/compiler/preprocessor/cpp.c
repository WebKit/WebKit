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
// cpp.c
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "compiler/preprocessor/slglobals.h"

#if defined(_MSC_VER)
#pragma warning(disable: 4054)
#pragma warning(disable: 4152)
#pragma warning(disable: 4706)
#endif

static int CPPif(yystypepp * yylvalpp);

/* Don't use memory.c's replacements, as we clean up properly here */
#undef malloc
#undef free

static int bindAtom = 0;
static int constAtom = 0;
static int defaultAtom = 0;
static int defineAtom = 0;
static int definedAtom = 0;
static int elseAtom = 0;
static int elifAtom = 0;
static int endifAtom = 0;
static int ifAtom = 0;
static int ifdefAtom = 0;
static int ifndefAtom = 0;
static int includeAtom = 0;
static int lineAtom = 0;
static int pragmaAtom = 0;
static int texunitAtom = 0;
static int undefAtom = 0;
static int errorAtom = 0;
static int __LINE__Atom = 0;
static int __FILE__Atom = 0;
static int __VERSION__Atom = 0;
static int versionAtom = 0;
static int extensionAtom = 0;

static Scope *macros = 0;
#define MAX_MACRO_ARGS  64

static SourceLoc ifloc; /* outermost #if */

int InitCPP(void)
{
    char        buffer[64], *t;
    const char  *f;

    // Add various atoms needed by the CPP line scanner:
    bindAtom = LookUpAddString(atable, "bind");
    constAtom = LookUpAddString(atable, "const");
    defaultAtom = LookUpAddString(atable, "default");
    defineAtom = LookUpAddString(atable, "define");
    definedAtom = LookUpAddString(atable, "defined");
    elifAtom = LookUpAddString(atable, "elif");
    elseAtom = LookUpAddString(atable, "else");
    endifAtom = LookUpAddString(atable, "endif");
    ifAtom = LookUpAddString(atable, "if");
    ifdefAtom = LookUpAddString(atable, "ifdef");
    ifndefAtom = LookUpAddString(atable, "ifndef");
    includeAtom = LookUpAddString(atable, "include");
    lineAtom = LookUpAddString(atable, "line");
    pragmaAtom = LookUpAddString(atable, "pragma");
    texunitAtom = LookUpAddString(atable, "texunit");
    undefAtom = LookUpAddString(atable, "undef");
	errorAtom = LookUpAddString(atable, "error");
    __LINE__Atom = LookUpAddString(atable, "__LINE__");
    __FILE__Atom = LookUpAddString(atable, "__FILE__");
	__VERSION__Atom = LookUpAddString(atable, "__VERSION__");
    versionAtom = LookUpAddString(atable, "version");
    extensionAtom = LookUpAddString(atable, "extension");
    macros = NewScopeInPool(mem_CreatePool(0, 0));
    strcpy(buffer, "PROFILE_");
    t = buffer + strlen(buffer);
    f = cpp->options.profileString;
    while ((isalnum(*f) || *f == '_') && t < buffer + sizeof(buffer) - 1)
        *t++ = toupper(*f++);
    *t = 0;

    PredefineIntMacro("GL_ES", 1);
    PredefineIntMacro("GL_FRAGMENT_PRECISION_HIGH", 1);

	return 1;
} // InitCPP

int FreeCPP(void)
{
    if (macros)
    {
        mem_FreePool(macros->pool);
        macros = 0;
    }

    return 1;
}

int FinalCPP(void)
{
	if (cpp->ifdepth)
		CPPErrorToInfoLog("#if mismatch");
    return 1;
}

static int CPPdefine(yystypepp * yylvalpp)
{
    int token, name, args[MAX_MACRO_ARGS], argc;
    const char *message;
    MacroSymbol mac;
    Symbol *symb;
    SourceLoc dummyLoc;
    memset(&mac, 0, sizeof(mac));
    token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    if (token != CPP_IDENTIFIER) {
        CPPErrorToInfoLog("#define");
        return token;
    }
    name = yylvalpp->sc_ident;
    token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    if (token == '(' && !yylvalpp->sc_int) {
        // gather arguments
        argc = 0;
        do {
            token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
            if (argc == 0 && token == ')') break;
            if (token != CPP_IDENTIFIER) {
				CPPErrorToInfoLog("#define");
                return token;
            }
            if (argc < MAX_MACRO_ARGS)
                args[argc++] = yylvalpp->sc_ident;
            token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
        } while (token == ',');
        if (token != ')') {
            CPPErrorToInfoLog("#define");
            return token;
        }
        mac.argc = argc;
        mac.args = mem_Alloc(macros->pool, argc * sizeof(int));
        memcpy(mac.args, args, argc * sizeof(int));
        token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
	}
    mac.body = NewTokenStream(GetAtomString(atable, name), macros->pool);
    while (token != '\n') {
        if (token == '\\') {
            CPPErrorToInfoLog("The line continuation character (\\) is not part of the OpenGL ES Shading Language");
            return token;
        } else if (token <= 0) { // EOF or error
            CPPErrorToInfoLog("unexpected end of input in #define preprocessor directive - expected a newline");
            return 0;
        }
        RecordToken(mac.body, token, yylvalpp);
        token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    };

    symb = LookUpSymbol(macros, name);
    if (symb) {
        if (!symb->details.mac.undef) {
            // already defined -- need to make sure they are identical
            if (symb->details.mac.argc != mac.argc) goto error;
            for (argc=0; argc < mac.argc; argc++)
                if (symb->details.mac.args[argc] != mac.args[argc])
                    goto error;
            RewindTokenStream(symb->details.mac.body);
            RewindTokenStream(mac.body);
            do {
                int old_lval, old_token;
                old_token = ReadToken(symb->details.mac.body, yylvalpp);
                old_lval = yylvalpp->sc_int;
                token = ReadToken(mac.body, yylvalpp);
                if (token != old_token || yylvalpp->sc_int != old_lval) { 
                error:
                    StoreStr("Macro Redefined");
                    StoreStr(GetStringOfAtom(atable,name));
                    message=GetStrfromTStr();
                    DecLineNumber();
                    CPPShInfoLogMsg(message);
                    IncLineNumber();
                    ResetTString();
                    break; }
            } while (token > 0);
        }
        //FreeMacro(&symb->details.mac);
    } else {
        dummyLoc.file = 0;
        dummyLoc.line = 0;
        symb = AddSymbol(&dummyLoc, macros, name, MACRO_S);
    }
    symb->details.mac = mac;
    return '\n';
} // CPPdefine

static int CPPundef(yystypepp * yylvalpp)
{
    int token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    Symbol *symb;
	if(token == '\n'){
		CPPErrorToInfoLog("#undef");
	    return token;
    }
    if (token != CPP_IDENTIFIER)
          goto error;
    symb = LookUpSymbol(macros, yylvalpp->sc_ident);
    if (symb) {
        symb->details.mac.undef = 1;
    }
    token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    if (token != '\n') {
    error:
        CPPErrorToInfoLog("#undef");
    }
    return token;
} // CPPundef

/* CPPelse -- skip forward to appropriate spot.  This is actually used
** to skip to and #endif after seeing an #else, AND to skip to a #else,
** #elif, or #endif after a #if/#ifdef/#ifndef/#elif test was false
*/

static int CPPelse(int matchelse, yystypepp * yylvalpp)
{
    int atom,depth=0;
    int token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
	
	while (token > 0) {
        if (token != '#') {
            while (token != '\n') {
                token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
                if (token <= 0) { // EOF or error
                    CPPErrorToInfoLog("unexpected end of input in #else preprocessor directive - expected a newline");
                    return 0;
                }
            }
            token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
            continue;
        }
		if ((token = cpp->currentInput->scan(cpp->currentInput, yylvalpp)) != CPP_IDENTIFIER)
			continue;
        atom = yylvalpp->sc_ident;
        if (atom == ifAtom || atom == ifdefAtom || atom == ifndefAtom){
            depth++; cpp->ifdepth++; cpp->elsetracker++;
            if (cpp->ifdepth > MAX_IF_NESTING) {
                CPPErrorToInfoLog("max #if nesting depth exceeded");
                cpp->CompileError = 1;
                return 0;
            }
            // sanity check elsetracker
            if (cpp->elsetracker < 0 || cpp->elsetracker >= MAX_IF_NESTING) {
                CPPErrorToInfoLog("mismatched #if/#endif statements");
                cpp->CompileError = 1;
                return 0;
            }
            cpp->elsedepth[cpp->elsetracker] = 0;
        }
        else if (atom == endifAtom) {
            if(--depth<0){
                if (cpp->elsetracker)
                    --cpp->elsetracker;
                if (cpp->ifdepth) 
                    --cpp->ifdepth;
                break;
            }             
            --cpp->elsetracker;
            --cpp->ifdepth;
        }
        else if (((int)(matchelse) != 0)&& depth==0) {
			if (atom == elseAtom ) {
                token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
                if (token != '\n') {
                    CPPWarningToInfoLog("unexpected tokens following #else preprocessor directive - expected a newline");
                    while (token != '\n') {
                        token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
                        if (token <= 0) { // EOF or error
                            CPPErrorToInfoLog("unexpected end of input following #else preprocessor directive - expected a newline");
                            return 0;
                        }
                    }
                } 
				break;
			} 
			else if (atom == elifAtom) {
                /* we decrement cpp->ifdepth here, because CPPif will increment
                 * it and we really want to leave it alone */
				if (cpp->ifdepth){
					--cpp->ifdepth;
				    --cpp->elsetracker;
				}
                return CPPif(yylvalpp);
            }
		}
        else if((atom==elseAtom) && (!ChkCorrectElseNesting())){
            CPPErrorToInfoLog("#else after a #else");
            cpp->CompileError=1;
            return 0;
        }
	};
    return token;
}

enum eval_prec {
    MIN_PREC,
    COND, LOGOR, LOGAND, OR, XOR, AND, EQUAL, RELATION, SHIFT, ADD, MUL, UNARY,
    MAX_PREC
};

static int op_logor(int a, int b) { return a || b; }
static int op_logand(int a, int b) { return a && b; }
static int op_or(int a, int b) { return a | b; }
static int op_xor(int a, int b) { return a ^ b; }
static int op_and(int a, int b) { return a & b; }
static int op_eq(int a, int b) { return a == b; }
static int op_ne(int a, int b) { return a != b; }
static int op_ge(int a, int b) { return a >= b; }
static int op_le(int a, int b) { return a <= b; }
static int op_gt(int a, int b) { return a > b; }
static int op_lt(int a, int b) { return a < b; }
static int op_shl(int a, int b) { return a << b; }
static int op_shr(int a, int b) { return a >> b; }
static int op_add(int a, int b) { return a + b; }
static int op_sub(int a, int b) { return a - b; }
static int op_mul(int a, int b) { return a * b; }
static int op_div(int a, int b) { return a / b; }
static int op_mod(int a, int b) { return a % b; }
static int op_pos(int a) { return a; }
static int op_neg(int a) { return -a; }
static int op_cmpl(int a) { return ~a; }
static int op_not(int a) { return !a; }

struct {
    int token, prec, (*op)(int, int);
} binop[] = {
    { CPP_OR_OP, LOGOR, op_logor },
    { CPP_AND_OP, LOGAND, op_logand },
    { '|', OR, op_or },
    { '^', XOR, op_xor },
    { '&', AND, op_and },
    { CPP_EQ_OP, EQUAL, op_eq },
    { CPP_NE_OP, EQUAL, op_ne },
    { '>', RELATION, op_gt },
    { CPP_GE_OP, RELATION, op_ge },
    { '<', RELATION, op_lt },
    { CPP_LE_OP, RELATION, op_le },
    { CPP_LEFT_OP, SHIFT, op_shl },
    { CPP_RIGHT_OP, SHIFT, op_shr },
    { '+', ADD, op_add },
    { '-', ADD, op_sub },
    { '*', MUL, op_mul },
    { '/', MUL, op_div },
    { '%', MUL, op_mod },
};

struct {
    int token, (*op)(int);
} unop[] = {
    { '+', op_pos },
    { '-', op_neg },
    { '~', op_cmpl },
    { '!', op_not },
};

#define ALEN(A) (sizeof(A)/sizeof(A[0]))

static int eval(int token, int prec, int *res, int *err, yystypepp * yylvalpp)
{
    int         i, val;
    Symbol      *s;
    if (token == CPP_IDENTIFIER) {
        if (yylvalpp->sc_ident == definedAtom) {
            int needclose = 0;
            token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
            if (token == '(') {
                needclose = 1;
                token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
            }
            if (token != CPP_IDENTIFIER)
                goto error;
            *res = (s = LookUpSymbol(macros, yylvalpp->sc_ident))
                        ? !s->details.mac.undef : 0;
            token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
            if (needclose) {
                if (token != ')')
                    goto error;
                token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
            }
		} else if (MacroExpand(yylvalpp->sc_ident, yylvalpp)) {
			token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
            return eval(token, prec, res, err, yylvalpp);
        } else {
            goto error;
        }
	} else if (token == CPP_INTCONSTANT) {
        *res = yylvalpp->sc_int;
        token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    } else if (token == '(') {
        token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
        token = eval(token, MIN_PREC, res, err, yylvalpp);
        if (!*err) {
            if (token != ')')
                goto error;
            token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
        }
    } else {
        for (i = ALEN(unop) - 1; i >= 0; i--) {
            if (unop[i].token == token)
                break;
        }
        if (i >= 0) {
            token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
            token = eval(token, UNARY, res, err, yylvalpp);
            *res = unop[i].op(*res);
        } else {
            goto error;
        }
    }
    while (!*err) {
        if (token == ')' || token == '\n') break;
        for (i = ALEN(binop) - 1; i >= 0; i--) {
            if (binop[i].token == token)
                break;
        }
        if (i < 0 || binop[i].prec <= prec)
            break;
        val = *res;
        token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
        token = eval(token, binop[i].prec, res, err, yylvalpp);
        
        if (binop[i].op == op_div || binop[i].op == op_mod)
        {
            if (*res == 0)
            {
                CPPErrorToInfoLog("preprocessor divide or modulo by zero");
                *err = 1;
                return token;
            }
        }

        *res = binop[i].op(val, *res);
    }
    return token;
error:
    CPPErrorToInfoLog("incorrect preprocessor directive");
    *err = 1;
    *res = 0;
    return token;
} // eval

static int CPPif(yystypepp * yylvalpp) {
    int token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    int res = 0, err = 0;

    if (!cpp->ifdepth++)
        ifloc = *cpp->tokenLoc;
    if(cpp->ifdepth > MAX_IF_NESTING){
        CPPErrorToInfoLog("max #if nesting depth exceeded");
        cpp->CompileError = 1;
        return 0;
    }
    cpp->elsetracker++;
    // sanity check elsetracker
    if (cpp->elsetracker < 0 || cpp->elsetracker >= MAX_IF_NESTING) {
        CPPErrorToInfoLog("mismatched #if/#endif statements");
        cpp->CompileError = 1;
        return 0;
    }
    cpp->elsedepth[cpp->elsetracker] = 0;

    token = eval(token, MIN_PREC, &res, &err, yylvalpp);
    if (token != '\n') {
        CPPWarningToInfoLog("unexpected tokens following #if preprocessor directive - expected a newline");
        while (token != '\n') {
            token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
            if (token <= 0) { // EOF or error
                CPPErrorToInfoLog("unexpected end of input in #if preprocessor directive - expected a newline");
                return 0;
            }
        }
    } 
    if (!res && !err) {
        token = CPPelse(1, yylvalpp);
    }

    return token;
} // CPPif

static int CPPifdef(int defined, yystypepp * yylvalpp)
{
    int token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    int name = yylvalpp->sc_ident;
    if(++cpp->ifdepth > MAX_IF_NESTING){
        CPPErrorToInfoLog("max #if nesting depth exceeded");
        cpp->CompileError = 1;
        return 0;
    }
    cpp->elsetracker++;
    // sanity check elsetracker
    if (cpp->elsetracker < 0 || cpp->elsetracker >= MAX_IF_NESTING) {
        CPPErrorToInfoLog("mismatched #if/#endif statements");
        cpp->CompileError = 1;
        return 0;
    }
    cpp->elsedepth[cpp->elsetracker] = 0;

    if (token != CPP_IDENTIFIER) {
            defined ? CPPErrorToInfoLog("ifdef"):CPPErrorToInfoLog("ifndef");
    } else {
        Symbol *s = LookUpSymbol(macros, name);
        token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
        if (token != '\n') {
            CPPWarningToInfoLog("unexpected tokens following #ifdef preprocessor directive - expected a newline");
            while (token != '\n') {
                token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
                if (token <= 0) { // EOF or error
                    CPPErrorToInfoLog("unexpected end of input in #ifdef preprocessor directive - expected a newline");
                    return 0;
                }
            }
        }
        if (((s && !s->details.mac.undef) ? 1 : 0) != defined)
            token = CPPelse(1, yylvalpp);
    }
    return token;
} // CPPifdef

static int CPPline(yystypepp * yylvalpp) 
{
    int token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
	if(token=='\n'){
		DecLineNumber();
        CPPErrorToInfoLog("#line");
        IncLineNumber();
		return token;
	}
	else if (token == CPP_INTCONSTANT) {
		yylvalpp->sc_int=atoi(yylvalpp->symbol_name);
		SetLineNumber(yylvalpp->sc_int);
        token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
        
		if (token == CPP_INTCONSTANT) {
            yylvalpp->sc_int=atoi(yylvalpp->symbol_name);
			SetStringNumber(yylvalpp->sc_int);
            token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
			if(token!='\n')
				CPPErrorToInfoLog("#line");
        }
		else if (token == '\n'){
			return token;
		}
		else{
            CPPErrorToInfoLog("#line");
		}
	}
	else{
          CPPErrorToInfoLog("#line");
	}
    return token;
}

static int CPPerror(yystypepp * yylvalpp) {

	int token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    const char *message;
	
    while (token != '\n') {
        if (token <= 0){
            CPPErrorToInfoLog("unexpected end of input in #error preprocessor directive - expected a newline");
            return 0;
        }else if (token == CPP_FLOATCONSTANT || token == CPP_INTCONSTANT){
            StoreStr(yylvalpp->symbol_name);
		}else if(token == CPP_IDENTIFIER || token == CPP_STRCONSTANT){
			StoreStr(GetStringOfAtom(atable,yylvalpp->sc_ident));
		}else {
		    StoreStr(GetStringOfAtom(atable,token));
		}
		token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
	}
	DecLineNumber();
	//store this msg into the shader's information log..set the Compile Error flag!!!!
	message=GetStrfromTStr();
    CPPShInfoLogMsg(message);
    ResetTString();
    cpp->CompileError=1;
    IncLineNumber();
    return '\n';
}//CPPerror

static int CPPpragma(yystypepp * yylvalpp)
{
	char SrcStrName[2];
	char** allTokens;
	int tokenCount = 0;
	int maxTokenCount = 10;
	const char* SrcStr;
	int i;

	int token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
	
	if (token=='\n') {
		DecLineNumber();
        CPPErrorToInfoLog("#pragma");
        IncLineNumber();
	    return token;
	}
	
	allTokens = (char**)malloc(sizeof(char*) * maxTokenCount);	

	while (token != '\n') {
		if (tokenCount >= maxTokenCount) {
			maxTokenCount *= 2;
			allTokens = (char**)realloc((char**)allTokens, sizeof(char*) * maxTokenCount);
		}
		switch (token) {
		case CPP_IDENTIFIER:
			SrcStr = GetAtomString(atable, yylvalpp->sc_ident);
			allTokens[tokenCount] = (char*)malloc(strlen(SrcStr) + 1);
			strcpy(allTokens[tokenCount++], SrcStr);
			break;
		case CPP_INTCONSTANT:
			SrcStr = yylvalpp->symbol_name;
			allTokens[tokenCount] = (char*)malloc(strlen(SrcStr) + 1);
			strcpy(allTokens[tokenCount++], SrcStr);
			break;
		case CPP_FLOATCONSTANT:
			SrcStr = yylvalpp->symbol_name;
			allTokens[tokenCount] = (char*)malloc(strlen(SrcStr) + 1);
			strcpy(allTokens[tokenCount++], SrcStr);
			break;
		case -1:
            // EOF
            CPPShInfoLogMsg("#pragma directive must end with a newline");			
			return token;
		default:
			SrcStrName[0] = token;
			SrcStrName[1] = '\0';
			allTokens[tokenCount] = (char*)malloc(2);
			strcpy(allTokens[tokenCount++], SrcStrName);
		}
		token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
	}

	cpp->currentInput->ungetch(cpp->currentInput, token, yylvalpp);
	HandlePragma((const char**)allTokens, tokenCount);
	token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
	
	for (i = 0; i < tokenCount; ++i) {
		free (allTokens[i]);
	}
	free (allTokens);	

	return token;    
} // CPPpragma

#define ESSL_VERSION_NUMBER 100
#define ESSL_VERSION_STRING "100"

static int CPPversion(yystypepp * yylvalpp)
{

    int token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);

    if (cpp->pastFirstStatement == 1)
        CPPShInfoLogMsg("#version must occur before any other statement in the program");

    if(token=='\n'){
		DecLineNumber();
        CPPErrorToInfoLog("#version");
        IncLineNumber();
		return token;
	}
    if (token != CPP_INTCONSTANT)
        CPPErrorToInfoLog("#version");
	
    yylvalpp->sc_int=atoi(yylvalpp->symbol_name);
	//SetVersionNumber(yylvalpp->sc_int);
    
    if (yylvalpp->sc_int != ESSL_VERSION_NUMBER)
        CPPShInfoLogMsg("Version number not supported by ESSL");

    token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    
	if (token == '\n'){
		return token;
	}
	else{
        CPPErrorToInfoLog("#version");
	}
    return token;
} // CPPversion

static int CPPextension(yystypepp * yylvalpp)
{

    int token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    char extensionName[MAX_SYMBOL_NAME_LEN + 1];

    if(token=='\n'){
		DecLineNumber();
        CPPShInfoLogMsg("extension name not specified");
        IncLineNumber();
		return token;
	}

    if (token != CPP_IDENTIFIER)
        CPPErrorToInfoLog("#extension");
    
    strncpy(extensionName, GetAtomString(atable, yylvalpp->sc_ident), MAX_SYMBOL_NAME_LEN);
    extensionName[MAX_SYMBOL_NAME_LEN] = '\0';
	    
    token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    if (token != ':') {
        CPPShInfoLogMsg("':' missing after extension name");
        return token;
    }
    
    token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    if (token != CPP_IDENTIFIER) {
        CPPShInfoLogMsg("behavior for extension not specified");
        return token;
    }

    updateExtensionBehavior(extensionName, GetAtomString(atable, yylvalpp->sc_ident));

    token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
	if (token == '\n'){
		return token;
	}
	else{
        CPPErrorToInfoLog("#extension");
	}
    return token;
} // CPPextension

int readCPPline(yystypepp * yylvalpp)
{
    int token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
    const char *message;
    int isVersion = 0;

    if (token == CPP_IDENTIFIER) {
        if (yylvalpp->sc_ident == defineAtom) {
             token = CPPdefine(yylvalpp);
        } else if (yylvalpp->sc_ident == elseAtom) {
			 if(ChkCorrectElseNesting()){
                 if (!cpp->ifdepth ){
                     CPPErrorToInfoLog("#else mismatch");
                     cpp->CompileError=1;
                     return 0;
                 }
                 token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
                 if (token != '\n') {
                     CPPWarningToInfoLog("unexpected tokens following #else preprocessor directive - expected a newline");
                     while (token != '\n') {
                         token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
                         if (token <= 0) { // EOF or error
                             CPPErrorToInfoLog("unexpected end of input in #ifdef preprocessor directive - expected a newline");
                             return 0;
                         }
                     }
                 }
			     token = CPPelse(0, yylvalpp);
             }else{
                 CPPErrorToInfoLog("#else after a #else");
                 cpp->ifdepth = 0;
                 cpp->elsetracker = 0;
                 cpp->pastFirstStatement = 1;
                 cpp->CompileError = 1;
                 return 0;
             }
		} else if (yylvalpp->sc_ident == elifAtom) {
            if (!cpp->ifdepth){
                 CPPErrorToInfoLog("#elif mismatch");
                 cpp->CompileError=1;
                 return 0;
            } 
            // this token is really a dont care, but we still need to eat the tokens
            token = cpp->currentInput->scan(cpp->currentInput, yylvalpp); 
            while (token != '\n') {
                token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
                if (token <= 0) { // EOF or error
                    CPPErrorToInfoLog("unexpect tokens following #elif preprocessor directive - expected a newline");
                    cpp->CompileError = 1;
                    return 0;
                }
            }
		    token = CPPelse(0, yylvalpp);
        } else if (yylvalpp->sc_ident == endifAtom) {
             if (!cpp->ifdepth){
                 CPPErrorToInfoLog("#endif mismatch");
                 cpp->CompileError=1;
                 return 0;
             }
             else
                 --cpp->ifdepth;

             if (cpp->elsetracker)
                 --cpp->elsetracker;

	    } else if (yylvalpp->sc_ident == ifAtom) {
             token = CPPif(yylvalpp);
        } else if (yylvalpp->sc_ident == ifdefAtom) {
             token = CPPifdef(1, yylvalpp);
        } else if (yylvalpp->sc_ident == ifndefAtom) {
             token = CPPifdef(0, yylvalpp);
        } else if (yylvalpp->sc_ident == lineAtom) {
             token = CPPline(yylvalpp);
        } else if (yylvalpp->sc_ident == pragmaAtom) {
             token = CPPpragma(yylvalpp);
        } else if (yylvalpp->sc_ident == undefAtom) {
             token = CPPundef(yylvalpp);
        } else if (yylvalpp->sc_ident == errorAtom) {
             token = CPPerror(yylvalpp);
        } else if (yylvalpp->sc_ident == versionAtom) {
            token = CPPversion(yylvalpp);
            isVersion = 1;
        } else if (yylvalpp->sc_ident == extensionAtom) {
            token = CPPextension(yylvalpp);
        } else {
            StoreStr("Invalid Directive");
            StoreStr(GetStringOfAtom(atable,yylvalpp->sc_ident));
            message=GetStrfromTStr();
            CPPShInfoLogMsg(message);
            ResetTString();
        }
    }
    while (token != '\n' && token != 0 && token != EOF) {
		token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
	}
        
    cpp->pastFirstStatement = 1;

    return token;
} // readCPPline

void FreeMacro(MacroSymbol *s) {
    DeleteTokenStream(s->body);
}

void PredefineIntMacro(const char *name, int value) {
    SourceLoc location = {0};
    Symbol *symbol = NULL;
    MacroSymbol macro = {0};
    yystypepp val = {0};
    int atom = 0;

    macro.body = NewTokenStream(name, macros->pool);
    val.sc_int = value;
    sprintf(val.symbol_name, "%d", value);
    RecordToken(macro.body, CPP_INTCONSTANT, &val);
    atom = LookUpAddString(atable, name);
    symbol = AddSymbol(&location, macros, atom, MACRO_S);
    symbol->details.mac = macro;
}

static int eof_scan(InputSrc *in, yystypepp * yylvalpp) { return -1; }
static void noop(InputSrc *in, int ch, yystypepp * yylvalpp) { }

static void PushEofSrc() {
    InputSrc *in = malloc(sizeof(InputSrc));
    memset(in, 0, sizeof(InputSrc));
    in->scan = eof_scan;
    in->getch = eof_scan;
    in->ungetch = noop;
    in->prev = cpp->currentInput;
    cpp->currentInput = in;
}

static void PopEofSrc() {
    if (cpp->currentInput->scan == eof_scan) {
        InputSrc *in = cpp->currentInput;
        cpp->currentInput = in->prev;
        free(in);
    }
}

static TokenStream *PrescanMacroArg(TokenStream *a, yystypepp * yylvalpp) {
    int token;
    TokenStream *n;
    RewindTokenStream(a);
    do {
        token = ReadToken(a, yylvalpp);
        if (token == CPP_IDENTIFIER && LookUpSymbol(macros, yylvalpp->sc_ident))
            break;
    } while (token > 0);
    if (token <= 0) return a;
    n = NewTokenStream("macro arg", 0);
    PushEofSrc();
    ReadFromTokenStream(a, 0, 0);
    while ((token = cpp->currentInput->scan(cpp->currentInput, yylvalpp)) > 0) {
        if (token == CPP_IDENTIFIER && MacroExpand(yylvalpp->sc_ident, yylvalpp))
            continue;
        RecordToken(n, token, yylvalpp);
    }
    PopEofSrc();
    DeleteTokenStream(a);
    return n;
} // PrescanMacroArg

typedef struct MacroInputSrc {
    InputSrc    base;
    MacroSymbol *mac;
    TokenStream **args;
} MacroInputSrc;

/* macro_scan ---
** return the next token for a macro expanion, handling macro args 
*/
static int macro_scan(MacroInputSrc *in, yystypepp * yylvalpp) {
    int i;
    int token = ReadToken(in->mac->body, yylvalpp);
    if (token == CPP_IDENTIFIER) {
        for (i = in->mac->argc-1; i>=0; i--)
            if (in->mac->args[i] == yylvalpp->sc_ident) break;
        if (i >= 0) {
            ReadFromTokenStream(in->args[i], yylvalpp->sc_ident, 0);
            return cpp->currentInput->scan(cpp->currentInput, yylvalpp);
        }
    }
    if (token > 0) return token;
    in->mac->busy = 0;
    cpp->currentInput = in->base.prev;
    if (in->args) {
        for (i=in->mac->argc-1; i>=0; i--)
            DeleteTokenStream(in->args[i]);
        free(in->args);
    }
    free(in);
    return cpp->currentInput->scan(cpp->currentInput, yylvalpp);
} // macro_scan

/* MacroExpand
** check an identifier (atom) to see if it a macro that should be expanded.
** If it is, push an InputSrc that will produce the appropriate expansion
** and return TRUE.  If not, return FALSE.
*/

int MacroExpand(int atom, yystypepp * yylvalpp)
{
    Symbol              *sym = LookUpSymbol(macros, atom);
    MacroInputSrc       *in;
    int i,j, token, depth=0;
    const char *message;
	if (atom == __LINE__Atom) {
        yylvalpp->sc_int = GetLineNumber();
        sprintf(yylvalpp->symbol_name,"%d",yylvalpp->sc_int);
        UngetToken(CPP_INTCONSTANT, yylvalpp);
        return 1;
    }
    if (atom == __FILE__Atom) {
        yylvalpp->sc_int = GetStringNumber();
        sprintf(yylvalpp->symbol_name,"%d",yylvalpp->sc_int);
        UngetToken(CPP_INTCONSTANT, yylvalpp);
        return 1;
    }
	if (atom == __VERSION__Atom) {
        strcpy(yylvalpp->symbol_name,ESSL_VERSION_STRING);
        yylvalpp->sc_int = atoi(yylvalpp->symbol_name);
        UngetToken(CPP_INTCONSTANT, yylvalpp);
        return 1;
    }
    if (!sym || sym->details.mac.undef) return 0;
    if (sym->details.mac.busy) return 0;        // no recursive expansions
    in = malloc(sizeof(*in));
    memset(in, 0, sizeof(*in));
    in->base.scan = (void *)macro_scan;
    in->base.line = cpp->currentInput->line;
    in->base.name = cpp->currentInput->name;
    in->mac = &sym->details.mac;
    if (sym->details.mac.args) {
        token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
		if (token != '(') {
            UngetToken(token, yylvalpp);
            yylvalpp->sc_ident = atom;
            return 0;
        }
        in->args = malloc(in->mac->argc * sizeof(TokenStream *));
        for (i=0; i<in->mac->argc; i++)
            in->args[i] = NewTokenStream("macro arg", 0);
		i=0;j=0;
        do{
            depth = 0;
			while(1) {
                token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
                if (token <= 0) {
                    StoreStr("EOF in Macro ");
                    StoreStr(GetStringOfAtom(atable,atom));
                    message=GetStrfromTStr();
                    CPPShInfoLogMsg(message);
                    ResetTString();
                    return 1;
                }
                if((in->mac->argc==0) && (token!=')')) break;
                if (depth == 0 && (token == ',' || token == ')')) break;
                if (token == '(') depth++;
                if (token == ')') depth--;
                RecordToken(in->args[i], token, yylvalpp);
                j=1;
			}
            if (token == ')') {
                if((in->mac->argc==1) &&j==0)
                    break;
                i++;
                break;
            }
            i++;
		}while(i < in->mac->argc);

        if (i < in->mac->argc) {
            StoreStr("Too few args in Macro ");
            StoreStr(GetStringOfAtom(atable,atom));
            message=GetStrfromTStr();
            CPPShInfoLogMsg(message);
            ResetTString();
        } else if (token != ')') {
            depth=0;
			while (token >= 0 && (depth > 0 || token != ')')) {
                if (token == ')') depth--;
                token = cpp->currentInput->scan(cpp->currentInput, yylvalpp);
                if (token == '(') depth++;
            }
			
            if (token <= 0) {
                StoreStr("EOF in Macro ");
                StoreStr(GetStringOfAtom(atable,atom));
                message=GetStrfromTStr();
                CPPShInfoLogMsg(message);
                ResetTString();
                return 1;
            }
            StoreStr("Too many args in Macro ");
            StoreStr(GetStringOfAtom(atable,atom));
            message=GetStrfromTStr();
            CPPShInfoLogMsg(message);
            ResetTString();
		}
		for (i=0; i<in->mac->argc; i++) {
            in->args[i] = PrescanMacroArg(in->args[i], yylvalpp);
        }
    }
#if 0
    printf("  <%s:%d>found macro %s\n", GetAtomString(atable, loc.file),
           loc.line, GetAtomString(atable, atom));
    for (i=0; i<in->mac->argc; i++) {
        printf("\targ %s = '", GetAtomString(atable, in->mac->args[i]));
        DumpTokenStream(stdout, in->args[i]);
        printf("'\n");
    }
#endif
	/*retain the input source*/
    in->base.prev = cpp->currentInput;
    sym->details.mac.busy = 1;
    RewindTokenStream(sym->details.mac.body);
    cpp->currentInput = &in->base;
    return 1;
} // MacroExpand

int ChkCorrectElseNesting(void)
{
    // sanity check to make sure elsetracker is in a valid range
    if (cpp->elsetracker < 0 || cpp->elsetracker >= MAX_IF_NESTING) {
        return 0;
    }

    if (cpp->elsedepth[cpp->elsetracker] == 0) {
        cpp->elsedepth[cpp->elsetracker] = 1;
        return 1;
    }
    return 0;
}


