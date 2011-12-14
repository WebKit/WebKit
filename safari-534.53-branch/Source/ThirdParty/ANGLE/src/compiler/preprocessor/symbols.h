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
// symbols.h
//

#if !defined(__SYMBOLS_H)
#define __SYMBOLS_H 1

#include "compiler/preprocessor/memory.h"

typedef enum symbolkind {
   MACRO_S
} symbolkind;

// Typedefs for things defined here in "symbols.h":

typedef struct Scope_Rec Scope;
typedef struct Symbol_Rec Symbol;

typedef struct SymbolList_Rec {
    struct SymbolList_Rec *next;
    Symbol *symb;
} SymbolList;

struct Scope_Rec {
    Scope *next, *prev;     // doubly-linked list of all scopes
    Scope *parent;
    Scope *funScope;        // Points to base scope of enclosing function
    MemoryPool *pool;       // pool used for allocation in this scope
    Symbol *symbols;
    
	int level;              // 0 = super globals, 1 = globals, etc.

    // Only used at global scope (level 1):
    SymbolList *programs;   // List of programs for this compilation.
};


// Symbol table is a simple binary tree.

#include "compiler/preprocessor/cpp.h"        // to get MacroSymbol def

struct Symbol_Rec {
    Symbol *left, *right;
    Symbol *next;
    int name;       // Name atom
    SourceLoc loc;
    symbolkind kind;
    union {
        MacroSymbol mac;
    } details;
};

extern Scope *CurrentScope;
extern Scope *GlobalScope;
extern Scope *ScopeList;

Scope *NewScopeInPool(MemoryPool *);
#define NewScope()      NewScopeInPool(CurrentScope->pool)
void PushScope(Scope *fScope);
Scope *PopScope(void);
Symbol *NewSymbol(SourceLoc *loc, Scope *fScope, int name, symbolkind kind);
Symbol *AddSymbol(SourceLoc *loc, Scope *fScope, int atom, symbolkind kind);
Symbol *LookUpLocalSymbol(Scope *fScope, int atom);
Symbol *LookUpSymbol(Scope *fScope, int atom);
void CPPErrorToInfoLog(char *);


#endif // !defined(__SYMBOLS_H)

