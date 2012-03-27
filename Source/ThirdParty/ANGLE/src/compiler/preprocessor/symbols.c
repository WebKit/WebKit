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
// symbols.c
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "compiler/preprocessor/slglobals.h"

#if defined(_MSC_VER)
#pragma warning(disable: 4706)
#endif

///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Symbol Table Variables: ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

Scope *ScopeList = NULL;
Scope *CurrentScope = NULL;
Scope *GlobalScope = NULL;

static void unlinkScope(void *_scope) {
    Scope *scope = _scope;

    if (scope->next)
        scope->next->prev = scope->prev;
    if (scope->prev)
        scope->prev->next = scope->next;
    else
        ScopeList = scope->next;
}

/*
 * NewScope()
 *
 */
Scope *NewScopeInPool(MemoryPool *pool)
{
    Scope *lScope;

    lScope = mem_Alloc(pool, sizeof(Scope));
    lScope->pool = pool;
    lScope->parent = NULL;
    lScope->funScope = NULL;
    lScope->symbols = NULL;
    
    lScope->level = 0;

    lScope->programs = NULL;
    if ((lScope->next = ScopeList))
        ScopeList->prev = lScope;
    lScope->prev = 0;
    ScopeList = lScope;
    mem_AddCleanup(pool, unlinkScope, lScope);
    return lScope;
} // NewScope

/*
 * PushScope()
 *
 */

void PushScope(Scope *fScope)
{
    Scope *lScope;

    if (CurrentScope) {
        fScope->level = CurrentScope->level + 1;
        if (fScope->level == 1) {
            if (!GlobalScope) {
                /* HACK - CTD -- if GlobalScope==NULL and level==1, we're
                 * defining a function in the superglobal scope.  Things
                 * will break if we leave the level as 1, so we arbitrarily
                 * set it to 2 */
                fScope->level = 2;
            }
        }
        if (fScope->level >= 2) {
            lScope = fScope;
            while (lScope->level > 2)
                lScope = lScope->next;
            fScope->funScope = lScope;
        }
    } else {
        fScope->level = 0;
    }
    fScope->parent = CurrentScope;
    CurrentScope = fScope;
} // PushScope

/*
 * PopScope()
 *
 */

Scope *PopScope(void)
{
    Scope *lScope;

    lScope = CurrentScope;
    if (CurrentScope)
        CurrentScope = CurrentScope->parent;
    return lScope;
} // PopScope

/*
 * NewSymbol() - Allocate a new symbol node;
 *
 */

Symbol *NewSymbol(SourceLoc *loc, Scope *fScope, int name, symbolkind kind)
{
    Symbol *lSymb;
    char *pch;
    unsigned int ii;

    lSymb = (Symbol *) mem_Alloc(fScope->pool, sizeof(Symbol));
    lSymb->left = NULL;
    lSymb->right = NULL;
    lSymb->next = NULL;
    lSymb->name = name;
    lSymb->loc = *loc;
    lSymb->kind = kind;
    
    // Clear union area:

    pch = (char *) &lSymb->details;
    for (ii = 0; ii < sizeof(lSymb->details); ii++)
        *pch++ = 0;
    return lSymb;
} // NewSymbol

/*
 * lAddToTree() - Using a binary tree is not a good idea for basic atom values because they
 *         are generated in order.  We'll fix this later (by reversing the bit pattern).
 */

static void lAddToTree(Symbol **fSymbols, Symbol *fSymb)
{
    Symbol *lSymb;
    int lrev, frev;

    lSymb = *fSymbols;
    if (lSymb) {
        frev = GetReversedAtom(atable, fSymb->name);
        while (lSymb) {
            lrev = GetReversedAtom(atable, lSymb->name);
            if (lrev == frev) {
                CPPErrorToInfoLog("GetAtomString(atable, fSymb->name)");
                break;
            } else {
                if (lrev > frev) {
                    if (lSymb->left) {
                        lSymb = lSymb->left;
                    } else {
                        lSymb->left = fSymb;
                        break;
                    }
                } else {
                    if (lSymb->right) {
                        lSymb = lSymb->right;
                    } else {
                        lSymb->right = fSymb;
                        break;
                    }
                }
            }
        }
    } else {
        *fSymbols = fSymb;
    }
} // lAddToTree


/*
 * AddSymbol() - Add a variable, type, or function name to a scope.
 *
 */

Symbol *AddSymbol(SourceLoc *loc, Scope *fScope, int atom, symbolkind kind)
{
    Symbol *lSymb;

    if (!fScope)
        fScope = CurrentScope;
    lSymb = NewSymbol(loc, fScope, atom, kind);
    lAddToTree(&fScope->symbols, lSymb);
    return lSymb;
} // AddSymbol


/*********************************************************************************************/
/************************************ Symbol Semantic Functions ******************************/
/*********************************************************************************************/

/*
 * LookUpLocalSymbol()
 *
 */

Symbol *LookUpLocalSymbol(Scope *fScope, int atom)
{
    Symbol *lSymb;
    int rname, ratom;

    ratom = GetReversedAtom(atable, atom);
    if (!fScope)
        fScope = CurrentScope;
    lSymb = fScope->symbols;
    while (lSymb) {
        rname = GetReversedAtom(atable, lSymb->name);
        if (rname == ratom) {
            return lSymb;
        } else {
            if (rname > ratom) {
                lSymb = lSymb->left;
            } else {
                lSymb = lSymb->right;
            }
        }
    }
    return NULL;
} // LookUpLocalSymbol

/*
 * LookUpSymbol()
 *
 */

Symbol *LookUpSymbol(Scope *fScope, int atom)
{
    Symbol *lSymb;

    if (!fScope)
        fScope = CurrentScope;
    while (fScope) {
        lSymb = LookUpLocalSymbol(fScope, atom);
        if (lSymb)
            return lSymb;
        fScope = fScope->parent;
    }
    return NULL;
} // LookUpSymbol

