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

#ifndef BISON_PARSER_H
# define BISON_PARSER_H

#ifndef yystypepp
typedef struct {
    int    sc_int;
    float  sc_fval;
    int    sc_ident;
	char   symbol_name[MAX_SYMBOL_NAME_LEN+1];
} yystypepp;

# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	CPP_AND_OP	        257
# define	CPP_SUB_ASSIGN	    259
# define	CPP_MOD_ASSIGN	    260
# define	CPP_ADD_ASSIGN	261
# define	CPP_DIV_ASSIGN	262
# define	CPP_MUL_ASSIGN	263
# define	CPP_EQ_OP	        264
# define    CPP_XOR_OP         265 
# define	ERROR_SY	    266
# define	CPP_FLOATCONSTANT	267
# define	CPP_GE_OP	        268
# define	CPP_RIGHT_OP        269
# define	CPP_IDENTIFIER	    270
# define	CPP_INTCONSTANT	    271
# define	CPP_LE_OP	        272
# define	CPP_LEFT_OP	        273
# define	CPP_DEC_OP	274
# define	CPP_NE_OP	        275
# define	CPP_OR_OP	        276
# define	CPP_INC_OP	    277
# define	CPP_STRCONSTANT	    278
# define	CPP_TYPEIDENTIFIER	279

# define	FIRST_USER_TOKEN_SY	289

# define	CPP_RIGHT_ASSIGN	    280
# define	CPP_LEFT_ASSIGN	    281
# define	CPP_AND_ASSIGN	282
# define	CPP_OR_ASSIGN  	283
# define	CPP_XOR_ASSIGN	284
# define	CPP_LEFT_BRACKET	285
# define	CPP_RIGHT_BRACKET	286
# define	CPP_LEFT_BRACE	287
# define	CPP_RIGHT_BRACE	288

#endif /* not BISON_PARSER_H */
