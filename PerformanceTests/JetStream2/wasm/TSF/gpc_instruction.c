/*
 * Copyright (C) 2003, 2004, 2005, 2015 Filip Pizlo. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "gpc_internal.h"

#include "gpc_instruction_static_size.gen"
#include "gpc_instruction_size.gen"
#include "gpc_instruction_stack_effects.gen"
#include "gpc_instruction_to_string.gen"

/* some things are still not handled by the specification... */

tsf_bool_t gpc_instruction_for_all_back_branches(gpc_cell_t *inst,
                                                 gpc_target_cback_t cback,
                                                 void *arg) {
    switch (*inst) {
        case GPC_I_ADDCOMPJUMP:
            return cback(inst+2,arg);
        case GPC_I_TWO_ADDCOMPJUMP:
            return cback(inst+3,arg);
        case GPC_I_DECCOMPJUMP:
            return cback(inst+1,arg);
        default:
            break;
    }
    
    return tsf_true;
}

tsf_bool_t gpc_instruction_for_all_forward_branches(gpc_cell_t *inst,
                                                    gpc_target_cback_t cback,
                                                    void *arg) {
    gpc_cell_t i;

    switch (*inst) {
        case GPC_I_COMPFAILJUMP_2ND:
        case GPC_I_COMPFAILJUMP:
        case GPC_I_ZEROJUMP:
            return cback(inst+1,arg);
        case GPC_I_TABLEJUMP_LOCAL:
            for (i=0;i<inst[1];++i) {
                if (!cback(inst+2+i,arg)) {
                    return tsf_false;
                }
            }
            break;
        case GPC_I_TABLEJUMP_FIELD:
            for (i=0;i<inst[2];++i) {
                if (!cback(inst+3+i,arg)) {
                    return tsf_false;
                }
            }
            break;
        case GPC_I_JUMP:
            return cback(inst+1,arg);
        default:
            break;
    }
    
    return tsf_true;
}

tsf_bool_t gpc_instruction_for_all_branches(gpc_cell_t *inst,
                                            gpc_target_cback_t cback,
                                            void *arg) {
    if (!gpc_instruction_for_all_back_branches(inst,cback,arg)) {
        return tsf_false;
    }
    if (!gpc_instruction_for_all_forward_branches(inst,cback,arg)) {
        return tsf_false;
    }
    return tsf_true;
}

tsf_bool_t gpc_instruction_is_return(gpc_cell_t inst) {
    switch (inst) {
    case GPC_I_RETURN_ONE:
    case GPC_I_RETURN_TOP_WITH_REGION:
    case GPC_I_RETURN_ONE_INIT_BUF_WITH_EMPTY_TYPES:
    case GPC_I_RETURN_ONE_INIT_BUF_WITH_TYPES_FROM_OUT_MAP:
        return tsf_true;
    default:
        return tsf_false;
    }
}

tsf_bool_t gpc_instruction_is_continuous(gpc_cell_t inst) {
    return !gpc_instruction_is_return(inst) &&
           inst != GPC_I_JUMP;
}


