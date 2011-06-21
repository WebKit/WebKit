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
// cppstruct.c
//

#include <stdio.h>
#include <stdlib.h>

#include "compiler/preprocessor/slglobals.h"

CPPStruct  *cpp      = NULL;
static int  refCount = 0;

int InitPreprocessor(void);
int ResetPreprocessor(void);
int FreeCPPStruct(void);
int FinalizePreprocessor(void);

/*
 * InitCPPStruct() - Initilaize the CPP structure.
 *
 */

int InitCPPStruct(void)
{
    int len;
    char *p;

    cpp = (CPPStruct *) malloc(sizeof(CPPStruct));
    if (cpp == NULL)
        return 0;

    refCount++;

    // Initialize public members:
    cpp->pLastSourceLoc = &cpp->lastSourceLoc;
    
	p = (char *) &cpp->options;
    len = sizeof(cpp->options);
    while (--len >= 0)
        p[len] = 0;
     
    ResetPreprocessor();
    return 1;
} // InitCPPStruct

int ResetPreprocessor(void)
{
    // Initialize private members:

    cpp->lastSourceLoc.file = 0;
    cpp->lastSourceLoc.line = 0;
    cpp->pC = 0;
    cpp->CompileError = 0;
    cpp->ifdepth = 0;
    for(cpp->elsetracker = 0; cpp->elsetracker < MAX_IF_NESTING; cpp->elsetracker++)
        cpp->elsedepth[cpp->elsetracker] = 0;
    cpp->elsetracker = 0;
    cpp->tokensBeforeEOF = 0;
    return 1;
}

//Intializing the Preprocessor.

int InitPreprocessor(void)
{
   #  define CPP_STUFF true
        #  ifdef CPP_STUFF
            FreeCPPStruct();
            InitCPPStruct();
            cpp->options.Quiet = 1;
            cpp->options.profileString = "generic";
            if (!InitAtomTable(atable, 0))
                return 1;
            if (!InitScanner(cpp))
	            return 1;
       #  endif
  return 0; 
}

//FreeCPPStruct() - Free the CPP structure.

int FreeCPPStruct(void)
{
    if (refCount)
    {
       free(cpp);
       refCount--;
    }
    
    return 1;
}

//Finalizing the Preprocessor.

int FinalizePreprocessor(void)
{
   #  define CPP_STUFF true
        #  ifdef CPP_STUFF
            FreeAtomTable(atable);
            FreeCPPStruct();
            FreeScanner();
       #  endif
  return 0; 
}


///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////// End of cppstruct.c //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
