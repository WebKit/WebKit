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
// tokens.c
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "compiler/debug.h"
#include "compiler/preprocessor/slglobals.h"
#include "compiler/util.h"

///////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// Preprocessor and Token Recorder and Playback: ////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

/*
 * idstr()
 * Copy a string to a malloc'ed block and convert it into something suitable
 * for an ID
 *
 */

static char *idstr(const char *fstr, MemoryPool *pool)
{
    size_t len;
    char *str, *t;
    const char *f;

    len = strlen(fstr);
    if (!pool)
        str = (char *) malloc(len + 1);
    else
        str = (char *) mem_Alloc(pool, len + 1);
    
    for (f=fstr, t=str; *f; f++) {
        if (isalnum(*f)) *t++ = *f;
        else if (*f == '.' || *f == '/') *t++ = '_';
    }
    *t = 0;
    return str;
} // idstr


/*
 * lNewBlock()
 *
 */

static TokenBlock *lNewBlock(TokenStream *fTok, MemoryPool *pool)
{
    TokenBlock *lBlock;

    if (!pool)
        lBlock = (TokenBlock *) malloc(sizeof(TokenBlock) + 256);
    else
        lBlock = (TokenBlock *) mem_Alloc(pool, sizeof(TokenBlock) + 256);
    lBlock->count = 0;
    lBlock->current = 0;
    lBlock->data = (unsigned char *) lBlock + sizeof(TokenBlock);
    lBlock->max = 256;
    lBlock->next = NULL;
    if (fTok->head) {
        fTok->current->next = lBlock;
    } else {
        fTok->head = lBlock;
    }
    fTok->current = lBlock;
    return lBlock;
} // lNewBlock

/*
 * lAddByte()
 *
 */

static void lAddByte(TokenStream *fTok, unsigned char fVal)
{
    TokenBlock *lBlock;
    lBlock = fTok->current;
    if (lBlock->count >= lBlock->max)
        lBlock = lNewBlock(fTok, 0);
    lBlock->data[lBlock->count++] = fVal;
} // lAddByte



/*
 * lReadByte() - Get the next byte from a stream.
 *
 */

static int lReadByte(TokenStream *pTok)
{
    TokenBlock *lBlock;
    int lval = -1;

    lBlock = pTok->current;
    if (lBlock) {
        if (lBlock->current >= lBlock->count) {
            lBlock = lBlock->next;
            if (lBlock)
                lBlock->current = 0;
            pTok->current = lBlock;
        }
        if (lBlock)
            lval = lBlock->data[lBlock->current++];
    }
    return lval;
} // lReadByte

/////////////////////////////////////// Global Functions://////////////////////////////////////

/*
 * NewTokenStream()
 *
 */

TokenStream *NewTokenStream(const char *name, MemoryPool *pool)
{
    TokenStream *pTok;

    if (!pool)
        pTok = (TokenStream *) malloc(sizeof(TokenStream));
    else
        pTok = (TokenStream*)mem_Alloc(pool, sizeof(TokenStream));
    pTok->next = NULL;
    pTok->name = idstr(name, pool);
    pTok->head = NULL;
    pTok->current = NULL;
    lNewBlock(pTok, pool);
    return pTok;
} // NewTokenStream

/*
 * DeleteTokenStream()
 *
 */

void DeleteTokenStream(TokenStream *pTok)
{
    TokenBlock *pBlock, *nBlock;

    if (pTok) {
        pBlock = pTok->head;
        while (pBlock) {
            nBlock = pBlock->next;
            free(pBlock);
            pBlock = nBlock;
        }
        if (pTok->name)
            free(pTok->name);
        free(pTok);
    }
} // DeleteTokenStream

/*
 * RecordToken() - Add a token to the end of a list for later playback or printout.
 *
 */

void RecordToken(TokenStream *pTok, int token, yystypepp * yylvalpp)
{
    const char *s;
    char *str=NULL;

    if (token > 256)
        lAddByte(pTok, (unsigned char)((token & 0x7f) + 0x80));
    else
        lAddByte(pTok, (unsigned char)(token & 0x7f));
    switch (token) {
    case CPP_IDENTIFIER:
    case CPP_TYPEIDENTIFIER:
    case CPP_STRCONSTANT:
        s = GetAtomString(atable, yylvalpp->sc_ident);
        while (*s)
            lAddByte(pTok, (unsigned char) *s++);
        lAddByte(pTok, 0);
        break;
    case CPP_FLOATCONSTANT:
    case CPP_INTCONSTANT:
         str=yylvalpp->symbol_name;
         while (*str){
            lAddByte(pTok, (unsigned char) *str++);
         }
         lAddByte(pTok, 0);
         break;
    case '(':
        lAddByte(pTok, (unsigned char)(yylvalpp->sc_int ? 1 : 0));
    default:
        break;
    }
} // RecordToken

/*
 * RewindTokenStream() - Reset a token stream in preperation for reading.
 *
 */

void RewindTokenStream(TokenStream *pTok)
{
    if (pTok->head) {
        pTok->current = pTok->head;
        pTok->current->current = 0;
    }
} // RewindTokenStream

/*
 * ReadToken() - Read the next token from a stream.
 *
 */

int ReadToken(TokenStream *pTok, yystypepp * yylvalpp)
{
    char symbol_name[MAX_SYMBOL_NAME_LEN + 1];
    char string_val[MAX_STRING_LEN + 1];
    int ltoken, len;
    char ch;

    ltoken = lReadByte(pTok);
    if (ltoken >= 0) {
        if (ltoken > 127)
            ltoken += 128;
        switch (ltoken) {
        case CPP_IDENTIFIER:
        case CPP_TYPEIDENTIFIER:
            len = 0;
            ch = lReadByte(pTok);
            while ((ch >= 'a' && ch <= 'z') ||
                     (ch >= 'A' && ch <= 'Z') ||
                     (ch >= '0' && ch <= '9') ||
                     ch == '_')
            {
                if (len < MAX_SYMBOL_NAME_LEN) {
                    symbol_name[len++] = ch;
                    ch = lReadByte(pTok);
                }
            }
            symbol_name[len] = '\0';
            assert(ch == '\0');
            yylvalpp->sc_ident = LookUpAddString(atable, symbol_name);
            return CPP_IDENTIFIER;
            break;
        case CPP_STRCONSTANT:
            len = 0;
            while ((ch = lReadByte(pTok)) != 0)
                if (len < MAX_STRING_LEN)
                    string_val[len++] = ch;
            string_val[len] = '\0';
            yylvalpp->sc_ident = LookUpAddString(atable, string_val);
            break;
        case CPP_FLOATCONSTANT:
            len = 0;
            ch = lReadByte(pTok);
            while ((ch >= '0' && ch <= '9')||(ch=='e'||ch=='E'||ch=='.')||(ch=='+'||ch=='-'))
            {
                if (len < MAX_SYMBOL_NAME_LEN) {
                    symbol_name[len++] = ch;
                    ch = lReadByte(pTok);
                }
            }
            symbol_name[len] = '\0';
            assert(ch == '\0');
            strcpy(yylvalpp->symbol_name,symbol_name);
            yylvalpp->sc_fval=(float)atof_dot(yylvalpp->symbol_name);
            break;
        case CPP_INTCONSTANT:
            len = 0;
            ch = lReadByte(pTok);
            while ((ch >= '0' && ch <= '9'))
            {
                if (len < MAX_SYMBOL_NAME_LEN) {
                    symbol_name[len++] = ch;
                    ch = lReadByte(pTok);
                }
            }
            symbol_name[len] = '\0';
            assert(ch == '\0');
            strcpy(yylvalpp->symbol_name,symbol_name);
            yylvalpp->sc_int=atoi(yylvalpp->symbol_name);
            break;
        case '(':
            yylvalpp->sc_int = lReadByte(pTok);
            break;
        }
        return ltoken;
    }
    return EOF_SY;
} // ReadToken

typedef struct TokenInputSrc {
    InputSrc            base;
    TokenStream         *tokens;
    int                 (*final)(CPPStruct *);
} TokenInputSrc;

static int scan_token(TokenInputSrc *in, yystypepp * yylvalpp)
{
    int token = ReadToken(in->tokens, yylvalpp);
    int (*final)(CPPStruct *);
    cpp->tokenLoc->file = cpp->currentInput->name;
    cpp->tokenLoc->line = cpp->currentInput->line;
    if (token == '\n') {
        in->base.line++;
        return token;
    }
    if (token > 0) return token;
    cpp->currentInput = in->base.prev;
    final = in->final;
    free(in);
    if (final && !final(cpp)) return -1;
    return cpp->currentInput->scan(cpp->currentInput, yylvalpp);
}

int ReadFromTokenStream(TokenStream *ts, int name, int (*final)(CPPStruct *))
{
    TokenInputSrc *in = malloc(sizeof(TokenInputSrc));
    memset(in, 0, sizeof(TokenInputSrc));
    in->base.name = name;
    in->base.prev = cpp->currentInput;
    in->base.scan = (int (*)(InputSrc *, yystypepp *))scan_token;
    in->base.line = 1;
    in->tokens = ts;
    in->final = final;
    RewindTokenStream(ts);
    cpp->currentInput = &in->base;
    return 1;
}

typedef struct UngotToken {
    InputSrc    base;
    int         token;
    yystypepp     lval;
} UngotToken;

static int reget_token(UngotToken *t, yystypepp * yylvalpp)
{
    int token = t->token;
    *yylvalpp = t->lval;
    cpp->currentInput = t->base.prev;
    free(t);
    return token;
}

void UngetToken(int token, yystypepp * yylvalpp) {
    UngotToken *t = malloc(sizeof(UngotToken));
    memset(t, 0, sizeof(UngotToken));
    t->token = token;
    t->lval = *yylvalpp;
    t->base.scan = (void *)reget_token;
    t->base.prev = cpp->currentInput;
    t->base.name = cpp->currentInput->name;
    t->base.line = cpp->currentInput->line;
    cpp->currentInput = &t->base;
}


void DumpTokenStream(FILE *fp, TokenStream *s, yystypepp * yylvalpp) {
    int token;
    char str[100];

    if (fp == 0) fp = stdout;
    RewindTokenStream(s);
    while ((token = ReadToken(s, yylvalpp)) > 0) {
        switch (token) {
        case CPP_IDENTIFIER:
        case CPP_TYPEIDENTIFIER:
            sprintf(str, "%s ", GetAtomString(atable, yylvalpp->sc_ident));
            break;
        case CPP_STRCONSTANT:
            sprintf(str, "\"%s\"", GetAtomString(atable, yylvalpp->sc_ident));
            break;
        case CPP_FLOATCONSTANT:
            //printf("%g9.6 ", yylvalpp->sc_fval);
            break;
        case CPP_INTCONSTANT:
            //printf("%d ", yylvalpp->sc_int);
            break;
        default:
            if (token >= 127)
                sprintf(str, "%s ", GetAtomString(atable, token));
            else
                sprintf(str, "%c", token);
            break;
        }
        CPPDebugLogMsg(str);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// End of tokens.c ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
