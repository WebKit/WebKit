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
// atom.c
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "compiler/debug.h"
#include "compiler/preprocessor/slglobals.h"

#undef malloc
#undef realloc
#undef free

///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////// String table: //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

static const struct {
    int val;
    const char *str;
} tokens[] = {
    { CPP_AND_OP,         "&&" },
    { CPP_AND_ASSIGN,     "&=" },
    { CPP_SUB_ASSIGN,     "-=" },
    { CPP_MOD_ASSIGN,     "%=" },
    { CPP_ADD_ASSIGN,     "+=" },
    { CPP_DIV_ASSIGN,     "/=" },
    { CPP_MUL_ASSIGN,     "*=" },
    { CPP_RIGHT_BRACKET,  ":>" },
    { CPP_EQ_OP,          "==" },
    { CPP_XOR_OP,         "^^" }, 
    { CPP_XOR_ASSIGN,     "^=" }, 
    { CPP_FLOATCONSTANT,  "<float-const>" },
    { CPP_GE_OP,          ">=" },
    { CPP_RIGHT_OP,       ">>" },
    { CPP_RIGHT_ASSIGN,   ">>=" }, 
    { CPP_IDENTIFIER,     "<ident>" },
    { CPP_INTCONSTANT,    "<int-const>" },
    { CPP_LE_OP,          "<=" },
    { CPP_LEFT_OP,        "<<" },
    { CPP_LEFT_ASSIGN,    "<<=" },
    { CPP_LEFT_BRACKET,   "<:" },
    { CPP_LEFT_BRACE,     "<%" }, 
    { CPP_DEC_OP,         "--" },
    { CPP_RIGHT_BRACE,    "%>" }, 
    { CPP_NE_OP,          "!=" },
    { CPP_OR_OP,          "||" },
    { CPP_OR_ASSIGN,      "|=" }, 
    { CPP_INC_OP,         "++" },
    { CPP_STRCONSTANT,    "<string-const>" },
    { CPP_TYPEIDENTIFIER, "<type-ident>" },
};

///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////// String table: //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

#define INIT_STRING_TABLE_SIZE 16384

typedef struct StringTable_Rec {
    char *strings;
    int nextFree;
    int size;
} StringTable;

/*
 * InitStringTable() - Initialize the string table.
 *
 */

static int InitStringTable(StringTable *stable)
{
    stable->strings = (char *) malloc(INIT_STRING_TABLE_SIZE);
    if (!stable->strings)
        return 0;
    // Zero-th offset means "empty" so don't use it.
    stable->nextFree = 1;
    stable->size = INIT_STRING_TABLE_SIZE;
    return 1;
} // InitStringTable

/*
 * FreeStringTable() - Free the string table.
 *
 */

static void FreeStringTable(StringTable *stable)
{
    if (stable->strings)
        free(stable->strings);
    stable->strings = NULL;
    stable->nextFree = 0;
    stable->size = 0;
} // FreeStringTable

/*
 * HashString() - Hash a string with the base hash function.
 *
 */

static int HashString(const char *s)
{
    int hval = 0;

    while (*s) {
        hval = (hval*13507 + *s*197) ^ (hval >> 2);
        s++;
    }
    return hval & 0x7fffffff;
} // HashString

/*
 * HashString2() - Hash a string with the incrimenting hash function.
 *
 */

static int HashString2(const char *s)
{
    int hval = 0;

    while (*s) {
        hval = (hval*729 + *s*37) ^ (hval >> 1);
        s++;
    }
    return hval;
} // HashString2

/*
 * AddString() - Add a string to a string table.  Return it's offset.
 *
 */

static int AddString(StringTable *stable, const char *s)
{
    int len, loc;
    char *str;

    len = (int) strlen(s);
    if (stable->nextFree + len + 1 >= stable->size) {
        assert(stable->size < 1000000);
        str = (char *) malloc(stable->size*2);
        memcpy(str, stable->strings, stable->size);
        free(stable->strings);
        stable->strings = str;
    }
    loc = stable->nextFree;
    strcpy(&stable->strings[loc], s);
    stable->nextFree += len + 1;
    return loc;
} // AddString

///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Hash table: ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

#define INIT_HASH_TABLE_SIZE 2047
#define HASH_TABLE_MAX_COLLISIONS 3

typedef struct HashEntry_Rec {
    int index;      // String table offset of string representation
    int value;      // Atom (symbol) value
} HashEntry;

typedef struct HashTable_Rec {
    HashEntry *entry;
    int size;
    int entries;
    int counts[HASH_TABLE_MAX_COLLISIONS + 1];
} HashTable;

/*
 * InitHashTable() - Initialize the hash table.
 *
 */

static int InitHashTable(HashTable *htable, int fsize)
{
    int ii;

    htable->entry = (HashEntry *) malloc(sizeof(HashEntry)*fsize);
    if (!htable->entry)
        return 0;
    htable->size = fsize;
    for (ii = 0; ii < fsize; ii++) {
        htable->entry[ii].index = 0;
        htable->entry[ii].value = 0;
    }
    htable->entries = 0;
    for (ii = 0; ii <= HASH_TABLE_MAX_COLLISIONS; ii++)
        htable->counts[ii] = 0;
    return 1;
} // InitHashTable

/*
 * FreeHashTable() - Free the hash table.
 *
 */

static void FreeHashTable(HashTable *htable)
{
    if (htable->entry)
        free(htable->entry);
    htable->entry = NULL;
    htable->size = 0;
    htable->entries = 0;
} // FreeHashTable

/*
 * Empty() - See if a hash table entry is empty.
 *
 */

static int Empty(HashTable *htable, int hashloc)
{
    assert(hashloc >= 0 && hashloc < htable->size);
    if (htable->entry[hashloc].index == 0) {
        return 1;
    } else {
        return 0;
    }
} // Empty

/*
 * Match() - See if a hash table entry is matches a string.
 *
 */

static int Match(HashTable *htable, StringTable *stable, const char *s, int hashloc)
{
    int strloc;

    strloc = htable->entry[hashloc].index;
    if (!strcmp(s, &stable->strings[strloc])) {
        return 1;
    } else {
        return 0;
    }
} // Match

///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Atom table: ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

#define INIT_ATOM_TABLE_SIZE 1024


struct AtomTable_Rec {
    StringTable stable; // String table.
    HashTable htable;   // Hashes string to atom number and token value.  Multiple strings can
                        // have the same token value but each unique string is a unique atom.
    int *amap;          // Maps atom value to offset in string table.  Atoms all map to unique
                        // strings except for some undefined values in the lower, fixed part
                        // of the atom table that map to "<undefined>".  The lowest 256 atoms
                        // correspond to single character ASCII values except for alphanumeric
                        // characters and '_', which can be other tokens.  Next come the
                        // language tokens with their atom values equal to the token value.
                        // Then come predefined atoms, followed by user specified identifiers.
    int *arev;          // Reversed atom for symbol table use.
    int nextFree;
    int size;
};

static AtomTable latable = { { 0 } };
AtomTable *atable = &latable;

static int AddAtomFixed(AtomTable *atable, const char *s, int atom);

/*
 * GrowAtomTable() - Grow the atom table to at least "size" if it's smaller.
 *
 */

static int GrowAtomTable(AtomTable *atable, int size)
{
    int *newmap, *newrev;

    if (atable->size < size) {
        if (atable->amap) {
            newmap = realloc(atable->amap, sizeof(int)*size);
            newrev = realloc(atable->arev, sizeof(int)*size);
        } else {
            newmap = malloc(sizeof(int)*size);
            newrev = malloc(sizeof(int)*size);
            atable->size = 0;
        }
        if (!newmap || !newrev) {
            /* failed to grow -- error */
            if (newmap)
                atable->amap = newmap;
            if (newrev)
                atable->amap = newrev;
            return -1;
        }
        memset(&newmap[atable->size], 0, (size - atable->size) * sizeof(int));
        memset(&newrev[atable->size], 0, (size - atable->size) * sizeof(int));
        atable->amap = newmap;
        atable->arev = newrev;
        atable->size = size;
    }
    return 0;
} // GrowAtomTable

/*
 * lReverse() - Reverse the bottom 20 bits of a 32 bit int.
 *
 */

static int lReverse(int fval)
{
    unsigned int in = fval;
    int result = 0, cnt = 0;

    while(in) {
        result <<= 1;
        result |= in&1;
        in >>= 1;
        cnt++;
    }

    // Don't use all 31 bits.  One million atoms is plenty and sometimes the
    // upper bits are used for other things.

    if (cnt < 20)
        result <<= 20 - cnt;
    return result;
} // lReverse

/*
 * AllocateAtom() - Allocate a new atom.  Associated with the "undefined" value of -1.
 *
 */

static int AllocateAtom(AtomTable *atable)
{
    if (atable->nextFree >= atable->size)
        GrowAtomTable(atable, atable->nextFree*2);
    atable->amap[atable->nextFree] = -1;
    atable->arev[atable->nextFree] = lReverse(atable->nextFree);
    atable->nextFree++;
    return atable->nextFree - 1;
} // AllocateAtom

/*
 * SetAtomValue() - Allocate a new atom associated with "hashindex".
 *
 */

static void SetAtomValue(AtomTable *atable, int atomnumber, int hashindex)
{
    atable->amap[atomnumber] = atable->htable.entry[hashindex].index;
    atable->htable.entry[hashindex].value = atomnumber;
} // SetAtomValue

/*
 * FindHashLoc() - Find the hash location for this string.  Return -1 it hash table is full.
 *
 */

static int FindHashLoc(AtomTable *atable, const char *s)
{
    int hashloc, hashdelta, count;
    int FoundEmptySlot = 0;
    int collision[HASH_TABLE_MAX_COLLISIONS + 1];

    hashloc = HashString(s) % atable->htable.size;
    if (!Empty(&atable->htable, hashloc)) {
        if (Match(&atable->htable, &atable->stable, s, hashloc))
            return hashloc;
        collision[0] = hashloc;
        hashdelta = HashString2(s);
        count = 0;
        while (count < HASH_TABLE_MAX_COLLISIONS) {
            hashloc = ((hashloc + hashdelta) & 0x7fffffff) % atable->htable.size;
            if (!Empty(&atable->htable, hashloc)) {
                if (Match(&atable->htable, &atable->stable, s, hashloc)) {
                    return hashloc;
                }
            } else {
                FoundEmptySlot = 1;
                break;
            }
            count++;
            collision[count] = hashloc;
        }

        if (!FoundEmptySlot) {
            if (cpp->options.DumpAtomTable) {
                int ii;
                char str[200];
                sprintf(str, "*** Hash failed with more than %d collisions. Must increase hash table size. ***",
                       HASH_TABLE_MAX_COLLISIONS);
                CPPShInfoLogMsg(str);

                sprintf(str, "*** New string \"%s\", hash=%04x, delta=%04x", s, collision[0], hashdelta);
                CPPShInfoLogMsg(str);
                for (ii = 0; ii <= HASH_TABLE_MAX_COLLISIONS; ii++) {
                    sprintf(str, "*** Collides on try %d at hash entry %04x with \"%s\"",
                           ii + 1, collision[ii], GetAtomString(atable, atable->htable.entry[collision[ii]].value));
                    CPPShInfoLogMsg(str);
                }
            }
            return -1;
        } else {
            atable->htable.counts[count]++;
        }
    }
    return hashloc;
} // FindHashLoc

/*
 * IncreaseHashTableSize()
 *
 */

static int IncreaseHashTableSize(AtomTable *atable)
{
    int ii, strloc, oldhashloc, value, size;
    AtomTable oldtable;
    char *s;

    // Save the old atom table and create a new one:

    oldtable = *atable;
    size = oldtable.htable.size*2 + 1;
    if (!InitAtomTable(atable, size))
        return 0;

    // Add all the existing values to the new atom table preserving their atom values:

    for (ii = atable->nextFree; ii < oldtable.nextFree; ii++) {
        strloc = oldtable.amap[ii];
        s = &oldtable.stable.strings[strloc];
        oldhashloc = FindHashLoc(&oldtable, s);
        assert(oldhashloc >= 0);
        value = oldtable.htable.entry[oldhashloc].value;
        AddAtomFixed(atable, s, value);
    }
    FreeAtomTable(&oldtable);
    return 1;
} // IncreaseHashTableSize

/*
 * LookUpAddStringHash() - Lookup a string in the hash table.  If it's not there, add it and
 *        initialize the atom value in the hash table to 0.  Return the hash table index.
 */

static int LookUpAddStringHash(AtomTable *atable, const char *s)
{
    int hashloc, strloc;

    while(1) {
        hashloc = FindHashLoc(atable, s);
        if (hashloc >= 0)
            break;
        IncreaseHashTableSize(atable);
    }

    if (Empty(&atable->htable, hashloc)) {
        atable->htable.entries++;
        strloc = AddString(&atable->stable, s);
        atable->htable.entry[hashloc].index = strloc;
        atable->htable.entry[hashloc].value = 0;
    }
    return hashloc;
} // LookUpAddStringHash

/*
 * LookUpAddString() - Lookup a string in the hash table.  If it's not there, add it and
 *        initialize the atom value in the hash table to the next atom number.
 *        Return the atom value of string.
 */

int LookUpAddString(AtomTable *atable, const char *s)
{
    int hashindex, atom;

    hashindex = LookUpAddStringHash(atable, s);
    atom = atable->htable.entry[hashindex].value;
    if (atom == 0) {
        atom = AllocateAtom(atable);
        SetAtomValue(atable, atom, hashindex);
    }
    return atom;
} // LookUpAddString

/*
 * GetAtomString()
 *
 */

const  char *GetAtomString(AtomTable *atable, int atom)
{
    int soffset;

    if (atom > 0 && atom < atable->nextFree) {
        soffset = atable->amap[atom];
        if (soffset > 0 && soffset < atable->stable.nextFree) {
            return &atable->stable.strings[soffset];
        } else {
            return "<internal error: bad soffset>";
        }
    } else {
        if (atom == 0) {
            return "<null atom>";
        } else {
            if (atom == EOF) {
                return "<EOF>";
            } else {
                return "<invalid atom>";
            }
        }
    }
} // GetAtomString

/*
 * GetReversedAtom()
 *
 */

int GetReversedAtom(AtomTable *atable, int atom)
{
    if (atom > 0 && atom < atable->nextFree) {
        return atable->arev[atom];
    } else {
        return 0;
    }
} // GetReversedAtom

/*
 * AddAtom() - Add a string to the atom, hash and string tables if it isn't already there.
 *         Return it's atom index.
 */

int AddAtom(AtomTable *atable, const char *s)
{
    int atom;

    atom = LookUpAddString(atable, s);
    return atom;
} // AddAtom

/*
 * AddAtomFixed() - Add an atom to the hash and string tables if it isn't already there.
 *         Assign it the atom value of "atom".
 */

static int AddAtomFixed(AtomTable *atable, const char *s, int atom)
{
    int hashindex, lsize;

    hashindex = LookUpAddStringHash(atable, s);
    if (atable->nextFree >= atable->size || atom >= atable->size) {
        lsize = atable->size*2;
        if (lsize <= atom)
            lsize = atom + 1;
        GrowAtomTable(atable, lsize);
    }
    atable->amap[atom] = atable->htable.entry[hashindex].index;
    atable->htable.entry[hashindex].value = atom;
    //if (atom >= atable->nextFree)
    //    atable->nextFree = atom + 1;
    while (atom >= atable->nextFree) {
        atable->arev[atable->nextFree] = lReverse(atable->nextFree);
        atable->nextFree++;
    }
    return atom;
} // AddAtomFixed

/*
 * InitAtomTable() - Initialize the atom table.
 *
 */

int InitAtomTable(AtomTable *atable, int htsize)
{
    unsigned int ii;

    htsize = htsize <= 0 ? INIT_HASH_TABLE_SIZE : htsize;
    if (!InitStringTable(&atable->stable))
        return 0;
    if (!InitHashTable(&atable->htable, htsize))
        return 0;

    atable->nextFree = 0;
    atable->amap = NULL;
    atable->size = 0;
    GrowAtomTable(atable, INIT_ATOM_TABLE_SIZE);
    if (!atable->amap)
        return 0;

    // Initialize lower part of atom table to "<undefined>" atom:

    AddAtomFixed(atable, "<undefined>", 0);
    for (ii = 0; ii < FIRST_USER_TOKEN_SY; ii++)
        atable->amap[ii] = atable->amap[0];

    // Add single character tokens to the atom table:

    {
		const char *s = "~!%^&*()-+=|,.<>/?;:[]{}#";
        char t[2];

        t[1] = '\0';
        while (*s) {
            t[0] = *s;
            AddAtomFixed(atable, t, s[0]);
            s++;
        }
    }

    // Add multiple character scanner tokens :

    for (ii = 0; ii < sizeof(tokens)/sizeof(tokens[0]); ii++)
        AddAtomFixed(atable, tokens[ii].str, tokens[ii].val);

    // Add error symbol if running in error mode:

    if (cpp->options.ErrorMode)
        AddAtomFixed(atable, "error", ERROR_SY);

    AddAtom(atable, "<*** end fixed atoms ***>");

    return 1;
} // InitAtomTable

///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Debug Printing Functions: //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

/*
 * PrintAtomTable()
 *
 */

void PrintAtomTable(AtomTable *atable)
{
    int ii;
    char str[200];

    for (ii = 0; ii < atable->nextFree; ii++) {
        sprintf(str, "%d: \"%s\"", ii, &atable->stable.strings[atable->amap[ii]]);
        CPPDebugLogMsg(str);
    }
    sprintf(str, "Hash table: size=%d, entries=%d, collisions=",
           atable->htable.size, atable->htable.entries);
    CPPDebugLogMsg(str);
    for (ii = 0; ii < HASH_TABLE_MAX_COLLISIONS; ii++) {
        sprintf(str, " %d", atable->htable.counts[ii]);
        CPPDebugLogMsg(str);
    }

} // PrintAtomTable


/*
 * GetStringOfAtom()
 *
 */

char* GetStringOfAtom(AtomTable *atable, int atom)
{
	 char* chr_str;
	 chr_str=&atable->stable.strings[atable->amap[atom]];
	 return chr_str;
} // GetStringOfAtom

/*
 * FreeAtomTable() - Free the atom table and associated memory
 *
 */

void FreeAtomTable(AtomTable *atable)
{
    FreeStringTable(&atable->stable);
    FreeHashTable(&atable->htable);
    if (atable->amap)
        free(atable->amap);
    if (atable->arev)
        free(atable->arev);
    atable->amap = NULL;
    atable->arev = NULL;
    atable->nextFree = 0;
    atable->size = 0;
} // FreeAtomTable

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////// End of atom.c ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

