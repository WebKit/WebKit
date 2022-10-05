/*
 * Copyright (C) 2003, 2004, 2005, 2007, 2014, 2015 Filip Pizlo. All rights reserved.
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

#include "tsf_internal.h"
#include "gpc.h"

#include <math.h>

#define C(exp) do {         \
    if (!(exp)) {           \
        return tsf_false;   \
    }                       \
} while(0)

#define append gpc_proto_append
#define append_tableset_local_to_field \
    gpc_proto_append_tableset_local_to_field
#define append_tableset_field_to_field \
    gpc_proto_append_tableset_field_to_field
#define append_tablejump_local gpc_proto_append_tablejump_local
#define append_tablejump_field gpc_proto_append_tablejump_field

static tsf_bool_t generate_size_calc(gpc_proto_t *proto,
                                     tsf_type_t *type,
                                     gpc_cell_t offset,
                                     gpc_cell_t label) {
    uint32_t i;
    uint32_t num_bits = 0;
    gpc_cell_t label_offset;
    gpc_cell_t done_label, really_done_label;
    gpc_cell_t *targets;
    tsf_bool_t result;
    
    gpc_cell_t static_size = tsf_type_get_static_size(type);
    if (static_size != UINT32_MAX) {
        if (static_size != 0) {
            C(append(proto, GPC_I_INC_SIZE, static_size));
        }
        
        return tsf_true;
    }
    
    switch (type->kind_code) {
    case TSF_TK_INTEGER:
        C(append(proto, GPC_I_TSF_INTEGER_SIZE, offset));
        break;
    case TSF_TK_LONG:
        C(append(proto, GPC_I_TSF_LONG_SIZE, offset));
        break;
    case TSF_TK_STRUCT:
        for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
            tsf_named_type_t *n = tsf_struct_type_get_element(type, i);
            if (n->type->kind_code == TSF_TK_BIT) {
                ++num_bits;
                continue;
            }
            C(generate_size_calc(proto,
                                 n->type,
                                 offset + n->offset,
                                 label));
        }
        if (num_bits != 0) {
            C(append(proto,
                     GPC_I_INC_SIZE,
                     (gpc_cell_t)((num_bits + 7) >> 3)));
        }
        break;
    case TSF_TK_ARRAY:
        if (type->u.a.element_type->kind_code == TSF_TK_VOID) {
            C(append(proto,
                     GPC_I_INC_SIZE_ARRAY_LEN,
                     offset + tsf_offsetof(tsf_native_void_array_t, len)));
        } else if (type->u.a.element_type->kind_code == TSF_TK_BIT) {
            C(append(proto,
                     GPC_I_INC_SIZE_BITVECTOR,
                     offset));
        } else {
            static_size = tsf_type_get_static_size(type->u.a.element_type);
            if (static_size == UINT32_MAX) {
                gpc_cell_t loop_label = label++;
                done_label = label++;
                    
                C(append(proto,
                         GPC_I_INC_SIZE_ARRAY_LEN,
                         offset + tsf_offsetof(tsf_native_array_t,
                                               len)));
                C(append(proto,
                         GPC_I_PUSH_PTR,
                         offset + tsf_offsetof(tsf_native_array_t,
                                               data)));
                C(append(proto,
                         GPC_I_REPUSH_MULADD_PTR,
                         offset + tsf_offsetof(tsf_native_array_t,
                                               len),
                         tsf_native_type_get_size(
                             type->u.a.element_type)));
                C(append(proto, GPC_I_COMPFAILJUMP, done_label));
                C(append(proto, GPC_I_LABEL, loop_label));
                C(generate_size_calc(proto,
                                     type->u.a.element_type,
                                     0,
                                     label));
                C(append(proto,
                         GPC_I_ADDCOMPJUMP,
                         tsf_native_type_get_size(
                             type->u.a.element_type),
                         loop_label));
                C(append(proto, GPC_I_LABEL, done_label));
                C(append(proto, GPC_I_TWO_POP));
            } else {
                C(append(proto,
                         GPC_I_INC_SIZE_ARRAY,
                         offset,
                         (gpc_cell_t)static_size));
            }
        }
        break;
    case TSF_TK_CHOICE:
        tsf_assert(tsf_choice_type_has_non_void(type) ||
                   (tsf_choice_type_get_num_elements(type) >= 256 &&
                    !type->u.h.choice_as_full_word));
        if (tsf_choice_type_get_num_elements(type) >= 256) {
            if (type->u.h.choice_as_full_word) {
                C(append(proto, GPC_I_INC_SIZE, 4));
            } else {
                C(append(proto, GPC_I_TSF_UNSIGNED_PLUS1_SIZE,
                         offset + type->u.h.value_offset));
            }
        } else {
            C(append(proto, GPC_I_INC_SIZE, 1));
        }
        if (tsf_choice_type_has_non_void(type)) {
            label_offset = label;
            label += tsf_choice_type_get_num_elements(type);
            done_label = label++;
            really_done_label = label++;
            targets=malloc(sizeof(gpc_cell_t)*
                           tsf_choice_type_get_num_elements(type));
            if (targets == NULL) {
                tsf_set_errno("Could not malloc array of "
                              "gpc_cell_t");
                return tsf_false;
            }
            for (i = 0; i < tsf_choice_type_get_num_elements(type); ++i) {
                targets[i] = label_offset + i;
            }
            result=append_tablejump_field(
                proto,
                offset + type->u.h.value_offset,
                tsf_choice_type_get_num_elements(type),
                targets);
            free(targets);
            if (!result) {
                return tsf_false;
            }
            C(append(proto, GPC_I_JUMP, really_done_label)); /* for unknown values */
            for (i = 0; i < tsf_choice_type_get_num_elements(type); ++i) {
                tsf_named_type_t *n = tsf_choice_type_get_element(type, i);
                C(append(proto, GPC_I_LABEL, label_offset + i));
                if (tsf_native_choice_type_is_in_place(type)) {
                    C(generate_size_calc(proto,
                                         n->type,
                                         offset + type->u.h.data_offset,
                                         label));
                } else {
                    /* cannot move this push above the tablejump since the
                       tablejump uses the value at the top of the stack, and
                       I don't feel like adding a second tablejump just to
                       get around that. */
                    C(append(proto, GPC_I_PUSH_PTR,
                             offset + type->u.h.data_offset));
                    C(generate_size_calc(proto,
                                         n->type,
                                         0,
                                         label));
                }
                C(append(proto, GPC_I_JUMP, done_label));
            }
            /* if it is in place, you get two labels...  oh well. */
            C(append(proto, GPC_I_LABEL, done_label));
            if (!tsf_native_choice_type_is_in_place(type)) {
                C(append(proto, GPC_I_POP));
            }
            C(append(proto, GPC_I_LABEL, really_done_label));
        }
        break;
    case TSF_TK_STRING:
        C(append(proto, GPC_I_STRING_SIZE, offset));
        break;
    case TSF_TK_ANY:
        C(append(proto, GPC_I_ANY_SIZE, offset));
        break;
    default:
        tsf_abort("Types other than struct, array, and choice "
                  "should have static size!");
        break;
    }
    
    return tsf_true;
}

static tsf_bool_t generate_generator(gpc_proto_t *proto,
                                     tsf_type_t *type,
                                     gpc_cell_t offset,
                                     gpc_cell_t label) {
    uint32_t i;
    uint32_t bit = 0;
    gpc_cell_t done_label, really_done_label;
    
    switch (type->kind_code) {
    case TSF_TK_VOID:
        break;
    case TSF_TK_INT8:
    case TSF_TK_UINT8:
        C(append(proto,
                 GPC_I_COPY_HTONC_INCDST,
                 offset));
        break;
    case TSF_TK_INT16:
    case TSF_TK_UINT16:
        C(append(proto,
                 GPC_I_COPY_HTONS_INCDST,
                 offset));
        break;
    case TSF_TK_INT32:
    case TSF_TK_UINT32:
        C(append(proto,
                 GPC_I_COPY_HTONL_INCDST,
                 offset));
        break;
    case TSF_TK_INT64:
    case TSF_TK_UINT64:
        C(append(proto,
                 GPC_I_COPY_HTONLL_INCDST,
                 offset));
        break;
    case TSF_TK_FLOAT:
        C(append(proto,
                 GPC_I_COPY_HTONF_INCDST,
                 offset));
        break;
    case TSF_TK_DOUBLE:
        C(append(proto,
                 GPC_I_COPY_HTOND_INCDST,
                 offset));
        break;
    case TSF_TK_BIT:
        C(append(proto,
                 GPC_I_BIT_WRITE,
                 offset));
        break;
    case TSF_TK_INTEGER:
        C(append(proto, GPC_I_TSF_INTEGER_WRITE, offset));
        break;
    case TSF_TK_LONG:
        C(append(proto, GPC_I_TSF_LONG_WRITE, offset));
        break;
    case TSF_TK_STRUCT:
        for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
            tsf_named_type_t *n = tsf_struct_type_get_element(type, i);
            if (n->type->kind_code == TSF_TK_BIT) {
                continue;
            }
            C(generate_generator(proto,
                                 n->type,
                                 offset + n->offset,
                                 label));
        }
        for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
            tsf_named_type_t *n = tsf_struct_type_get_element(type, i);
            if (n->type->kind_code != TSF_TK_BIT) {
                continue;
            }
            C(append(proto,
                     GPC_I_BIT_MASK_WRITE,
                     bit,
                     offset + n->offset));
            ++bit;
            if (bit == 8) {
                bit = 0;
                C(append(proto, GPC_I_SKIP, 1));
            }
        }
        if (bit) {
            C(append(proto, GPC_I_SKIP, 1));
        }
        break;
    case TSF_TK_ARRAY:
        if (type->u.a.element_type->kind_code == TSF_TK_VOID) {
            C(append(proto,
                     GPC_I_ARRAY_LEN_WRITE_FIELD,
                     offset + tsf_offsetof(tsf_native_void_array_t,
                                           len)));
        } else if (type->u.a.element_type->kind_code == TSF_TK_BIT) {
            C(append(proto,
                     GPC_I_BITVECTOR_WRITE,
                     offset));
        } else if (type->u.a.element_type->kind_code == TSF_TK_UINT8 ||
                   type->u.a.element_type->kind_code == TSF_TK_INT8
#ifndef NEED_INT_CONVERSION
                   || tsf_type_kind_is_int(
                       type->u.a.element_type->kind_code)
#endif
#ifndef NEED_FLOAT_CONVERSION
                   || tsf_type_kind_is_float(
                       type->u.a.element_type->kind_code)
#endif
            ) {
            C(append(proto,
                     GPC_I_BYTE_ARRAY_WRITE,
                     offset,
                     tsf_native_type_get_size(
                         type->u.a.element_type)));
        } else {
            gpc_cell_t loop_label = label++;
            done_label = label++;
                
            C(append(proto,
                     GPC_I_ARRAY_LEN_WRITE_FIELD,
                     offset + tsf_offsetof(tsf_native_array_t,
                                           len)));

            C(append(proto,
                     GPC_I_PUSH_PTR,
                     offset + tsf_offsetof(tsf_native_array_t,
                                           data)));
            C(append(proto,
                     GPC_I_REPUSH_MULADD_PTR,
                     offset + tsf_offsetof(tsf_native_array_t,
                                           len),
                     tsf_native_type_get_size(
                         type->u.a.element_type)));
            C(append(proto,
                     GPC_I_COMPFAILJUMP,
                     done_label));
            C(append(proto,
                     GPC_I_LABEL,
                     loop_label));
            C(generate_generator(proto,
                                 type->u.a.element_type,
                                 0,
                                 label));
            C(append(proto,
                     GPC_I_ADDCOMPJUMP,
                     tsf_native_type_get_size(
                         type->u.a.element_type),
                     loop_label));
            C(append(proto,
                     GPC_I_LABEL,
                     done_label));
            C(append(proto,GPC_I_TWO_POP));
        }
        break;
    case TSF_TK_CHOICE:
        if (tsf_choice_type_get_num_elements(type) >= 256) {
            if (type->u.h.choice_as_full_word) {
                C(append(proto,
                         GPC_I_COPY_HTONL_INCDST,
                         offset + type->u.h.value_offset));
            } else {
                C(append(proto,
                         GPC_I_TSF_UNSIGNED_PLUS1_WRITE,
                         offset + type->u.h.value_offset));
            }
        } else {
            C(append(proto,
                     GPC_I_COPY_HTONCHOICE_TO_UI8_INCDST,
                     offset + type->u.h.value_offset));
        }
        if (tsf_choice_type_has_non_void(type)) {
            gpc_cell_t *targets;
            gpc_cell_t label_offset;
            tsf_bool_t result;
                
            label_offset = label;
            label += tsf_choice_type_get_num_elements(type);
            done_label = label++;
            really_done_label = label++;
            targets = malloc(sizeof(gpc_cell_t) *
                             tsf_choice_type_get_num_elements(type));
            if (targets == NULL) {
                tsf_set_errno("Could not malloc array of gpc_cell_t");
                return tsf_false;
            }
            for (i = 0; i < tsf_choice_type_get_num_elements(type); ++i) {
                targets[i] = label_offset + i;
            }
            result = append_tablejump_field(
                proto,
                offset + type->u.h.value_offset,
                tsf_choice_type_get_num_elements(type),
                targets);
            free(targets);
            if (!result) {
                return tsf_false;
            }
                
            /* for unknown values */
            C(append(proto, GPC_I_JUMP, really_done_label));
                
            for (i = 0; i < tsf_choice_type_get_num_elements(type); ++i) {
                tsf_named_type_t *n = tsf_choice_type_get_element(type, i);
                C(append(proto, GPC_I_LABEL, label_offset + i));
                if (tsf_native_choice_type_is_in_place(type)) {
                    C(generate_generator(proto,
                                         n->type,
                                         offset + type->u.h.data_offset,
                                         label));
                } else {
                    C(append(proto, GPC_I_PUSH_PTR,
                             offset + type->u.h.data_offset));
                    C(generate_generator(proto,
                                         n->type,
                                         0,
                                         label));
                }
                C(append(proto, GPC_I_JUMP, done_label));
            }
            C(append(proto, GPC_I_LABEL, done_label));
            if (!tsf_native_choice_type_is_in_place(type)) {
                C(append(proto, GPC_I_POP));
            }
            C(append(proto, GPC_I_LABEL, really_done_label));
        }
        break;
    case TSF_TK_STRING:
        C(append(proto, GPC_I_STRING_WRITE, offset));
        break;
    case TSF_TK_ANY:
        C(append(proto, GPC_I_ANY_WRITE, offset));
        break;
    default:
        tsf_abort("Bad kind code!");
        break;
    }
    
    return tsf_true;
}

gpc_proto_t *tsf_gpc_code_gen_generate_generator(tsf_type_t *type) {
    gpc_proto_t *proto;
    
    if (!tsf_native_type_has_struct_mapping(type)) {
        tsf_set_error(TSF_E_NO_STRUCT_MAP, NULL);
        return NULL;
    }
    
    proto=gpc_proto_create(2);
    if (proto == NULL) {
        return NULL;
    }
    
    if (!generate_size_calc(proto, type, 0, 0)) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    if (!append(proto, GPC_I_MALLOC_BUF)) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    if (tsf_type_is_dynamic(type)) {
        if (!append(proto, GPC_I_MAKE_OUT_MAP)) {
            gpc_proto_destroy(proto);
            return NULL;
        }
    }
    
    if (!generate_generator(proto, type, 0, 0)) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    if (tsf_type_is_dynamic(type)) {
        if (!append(proto, GPC_I_RETURN_ONE_INIT_BUF_WITH_TYPES_FROM_OUT_MAP)) {
            gpc_proto_destroy(proto);
            return NULL;
        }
    } else {
        if (!append(proto, GPC_I_RETURN_ONE_INIT_BUF_WITH_EMPTY_TYPES)) {
            gpc_proto_destroy(proto);
            return NULL;
        }
    }
    
    return proto;
}

static tsf_bool_t generate_set_zero_ptr(gpc_proto_t *proto,
					gpc_cell_t offset) {
    if (sizeof(void*) == 4) {
	C(append(proto, GPC_I_SET_L, offset, 0));
    } else if (sizeof(void*) == 8) {
	C(append(proto, GPC_I_SET_L, offset, 0));
	C(append(proto, GPC_I_SET_L, offset+4, 0));
    } else {
	tsf_abort("weird pointer size");
    }
    return tsf_true;
}

static tsf_bool_t generate_set_default(gpc_proto_t *proto,
				       tsf_type_t *dest_type,
				       gpc_cell_t offset) {
    /* fill in default values */
    unsigned i;
	
    switch (dest_type->kind_code) {
    case TSF_TK_INT8:
    case TSF_TK_UINT8:
	C(append(proto, GPC_I_SET_C, offset, 0));
	break;
    case TSF_TK_INT16:
    case TSF_TK_UINT16:
	C(append(proto, GPC_I_SET_S, offset, 0));
	break;
    case TSF_TK_INT32:
    case TSF_TK_UINT32:
    case TSF_TK_INTEGER:
	C(append(proto, GPC_I_SET_L, offset, 0));
	break;
    case TSF_TK_INT64:
    case TSF_TK_UINT64:
    case TSF_TK_LONG:
	C(append(proto, GPC_I_SET_L, offset + 0, 0));
	C(append(proto, GPC_I_SET_L, offset + 4, 0));
	break;
    case TSF_TK_FLOAT: {
	union {
	    float f;
	    uint32_t i;
	} u;
	u.f = (float)log(-1.); /* NaN */
	C(append(proto, GPC_I_SET_L, offset, u.i));
	break;
    }
    case TSF_TK_DOUBLE: {
	union {
	    double d;
	    uint32_t i[2];
	} u;
	u.d = (double)log(-1.); /* NaN */
	C(append(proto, GPC_I_SET_L, offset + 0, u.i[0]));
	C(append(proto, GPC_I_SET_L, offset + 4, u.i[1]));
	break;
    }
    case TSF_TK_BIT:
	C(append(proto, GPC_I_SET_C, offset, 0));
	break;
    case TSF_TK_STRING:
	C(append(proto, GPC_I_STRING_SETEMPTY, offset));
	break;
    case TSF_TK_ARRAY:
	switch (dest_type->u.a.element_type->kind_code) {
	case TSF_TK_VOID:
	    C(append(proto, GPC_I_SET_L,
		     offset + tsf_offsetof(tsf_native_void_array_t, len),
		     0));
	    break;
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_SET_L,
		     offset + tsf_offsetof(tsf_native_bitvector_t, num_bits),
		     0));
	    C(generate_set_zero_ptr(proto,
				    offset + tsf_offsetof(tsf_native_bitvector_t,
                                                          bits)));
	    break;
	default:
	    C(append(proto, GPC_I_SET_L,
		     offset + tsf_offsetof(tsf_native_array_t, len),
		     0));
	    C(generate_set_zero_ptr(proto,
				    offset + tsf_offsetof(tsf_native_array_t,
                                                          data)));
	    break;
	}
	break;
    case TSF_TK_STRUCT:
	for (i = 0; i < tsf_struct_type_get_num_elements(dest_type); ++i) {
	    tsf_named_type_t *n = tsf_struct_type_get_element(dest_type, i);
	    C(generate_set_default(proto, n->type,
				   offset + n->offset));
	}
	break;
    case TSF_TK_CHOICE:
	C(append(proto, GPC_I_SET_L,
		 (gpc_cell_t)(offset + dest_type->u.h.value_offset),
		 (gpc_cell_t)UINT32_MAX));
	break;
    case TSF_TK_ANY:
	tsf_abort("not implemented");
	break;
    default:
	tsf_abort("bad kind code");
    }
    
    return tsf_true;
}

static tsf_bool_t generate_set_default_copier(gpc_proto_t *proto,
					      tsf_type_t *dest_type,
					      gpc_cell_t offset) {
    /* this can be optimized... */

    C(append(proto, GPC_I_DUP_PTR_2ND));
    C(generate_set_default(proto, dest_type, offset));
    C(append(proto, GPC_I_POP));
    
    return tsf_true;
}

static tsf_bool_t generate_parser(gpc_proto_t *proto,
                                  tsf_type_t *dest_type,
                                  tsf_type_t *src_type,
                                  gpc_cell_t offset,
                                  gpc_cell_t label,
                                  tsf_bool_t do_bounds_check) {
    uint32_t i, bit = 0, num_bits = 0;
    gpc_cell_t loop_label, done_label, really_done_label, label_offset;
    gpc_cell_t *targets;
    uint32_t static_size;
    tsf_bool_t result;
    tsf_bool_t instanceof;
    
    static_size = tsf_type_get_static_size(src_type);

    if (do_bounds_check && static_size != UINT32_MAX) {
        if (static_size > 0) {
            C(append(proto, GPC_I_BC, static_size));
        }
        do_bounds_check = tsf_false;
    }
    
    instanceof = tsf_type_instanceof(src_type, dest_type);
    
    if (!instanceof) {
	C(generate_set_default(proto, dest_type, offset));
    }
    
    if (dest_type->kind_code == TSF_TK_VOID || !instanceof) {
	/* this is the skip parser. */

        tsf_type_t *type = src_type;  /* makes things simpler since we
                                       * won't refer to dest_type at this
                                       * point. */
        
        if (static_size != UINT32_MAX) {
            if (static_size > 0) {
                C(append(proto,
                         GPC_I_SKIP,
                         (gpc_cell_t)static_size));
            }
            return tsf_true;
        }
        
        /* interesting observation: from here on, do_bounds_check must be true.
         * because if do_bounds_check was false, then that means that a bounds
         * check was possible, which means in turn that our size is statically
         * known. */
        tsf_assert(do_bounds_check);
        
        switch (type->kind_code) {
        case TSF_TK_INTEGER:
            C(append(proto, GPC_I_TSF_INTEGER_SKIP));
            break;
        case TSF_TK_LONG:
            C(append(proto, GPC_I_TSF_LONG_SKIP));
            break;
	case TSF_TK_STRUCT:
	    for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
		tsf_named_type_t *n = tsf_struct_type_get_element(type, i);
		if (n->type->kind_code == TSF_TK_BIT) {
		    num_bits++;
		    continue;
		}
		C(generate_parser(proto,
				  tsf_type_create(TSF_TK_VOID),
				  n->type,
				  0,
				  label,
				  tsf_true));
	    }
	    if (num_bits) {
		C(append(proto,
			 GPC_I_SKIP,
			 (gpc_cell_t)((num_bits + 7) >> 3)));
	    }
	    break;
	case TSF_TK_ARRAY:
	    if (type->u.a.element_type->kind_code == TSF_TK_VOID) {
		C(append(proto, GPC_I_ARRAY_LEN_SKIP));
	    } else if (type->u.a.element_type->kind_code == TSF_TK_BIT) {
		C(append(proto, GPC_I_BITVECTOR_SKIP));
	    } else {
		static_size =
		    tsf_type_get_static_size(type->u.a.element_type);
		if (static_size == UINT32_MAX) {
		    C(append(proto, GPC_I_ARRAY_LEN_READ_LOCAL));

		    loop_label = label++;
		    done_label = label++;
                        
		    C(append(proto, GPC_I_ZEROJUMP, done_label));
		    C(append(proto, GPC_I_LABEL, loop_label));
		    C(generate_parser(proto,
				      tsf_type_create(TSF_TK_VOID),
				      type->u.a.element_type,
				      0,
				      label,
				      tsf_true));
		    C(append(proto, GPC_I_DECCOMPJUMP, loop_label));
		    C(append(proto, GPC_I_LABEL, done_label));
		    C(append(proto, GPC_I_POP));
		} else {
		    C(append(proto,
			     GPC_I_ARRAY_SKIP,
			     (gpc_cell_t)static_size));
		}
	    }
	    break;
	case TSF_TK_CHOICE:
	    tsf_assert(tsf_choice_type_has_non_void(type) ||
                       (tsf_choice_type_get_num_elements(type) >= 256 &&
                        !type->u.h.choice_as_full_word));
                
	    if (tsf_choice_type_get_num_elements(type) >= 256) {
                if (type->u.h.choice_as_full_word) {
                    C(append(proto, GPC_I_BC, 4));
                    C(append(proto, GPC_I_READL));
                } else {
                    if (tsf_choice_type_has_non_void(type)) {
                        C(append(proto, GPC_I_TSF_UNSIGNED_READ_SUB1));
                    } else {
                        C(append(proto, GPC_I_TSF_UNSIGNED_SKIP));
                    }
                }
	    } else {
		C(append(proto, GPC_I_BC, 1));
		C(append(proto, GPC_I_READC_TO_CHOICE));
	    }
            
            if (tsf_choice_type_has_non_void(type)) {
                label_offset = label;
                label += tsf_choice_type_get_num_elements(type);
                done_label = label++;
                targets=malloc(sizeof(gpc_cell_t) *
                               tsf_choice_type_get_num_elements(type));
                if (targets == NULL) {
                    tsf_set_errno("Could not malloc array of "
                                  "gpc_cell_t");
                    return tsf_false;
                }
                for (i = 0; i < tsf_choice_type_get_num_elements(type); ++i) {
                    targets[i] = label_offset + i;
                }
                result = append_tablejump_local(
                    proto,
                    tsf_choice_type_get_num_elements(type),
                    targets);
                free(targets);
                if (!result) {
                    return tsf_false;
                }
                
                /* for unknown (default) values */
                C(append(proto, GPC_I_JUMP, done_label));
                
                for (i = 0; i < tsf_choice_type_get_num_elements(type); ++i) {
                    tsf_named_type_t *n = tsf_choice_type_get_element(type, i);
                    C(append(proto, GPC_I_LABEL, label_offset + i));
                    C(generate_parser(proto,
                                      tsf_type_create(TSF_TK_VOID),
                                      n->type,
                                      0,
                                      label,
                                      tsf_true));
                    C(append(proto, GPC_I_JUMP, done_label));
                }
                C(append(proto, GPC_I_LABEL, done_label));
            }
	    break;
	case TSF_TK_STRING:
	    C(append(proto, GPC_I_STRING_SKIP));
	    break;
	case TSF_TK_ANY:
	    C(append(proto, GPC_I_ANY_SKIP));
	    break;
	default:
	    tsf_abort("There appears to be a kind code that we don't "
		      "know about");
	    break;
        }
        
        return tsf_true;
    }
    
    switch (src_type->kind_code) {
    case TSF_TK_VOID:
	break;
    case TSF_TK_INT8:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_BIT_INCSRC, offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_COPY_NTOHC_INCSRC, offset));
	    break;
	case TSF_TK_UINT16:
	case TSF_TK_INT16:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_S_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT32:
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_L_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT64:
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_LL_E_INCSRC, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_F_E_INCSRC, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_D_E_INCSRC, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_UINT8:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_BIT_INCSRC, offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_COPY_NTOHC_INCSRC, offset));
	    break;
	case TSF_TK_UINT16:
	case TSF_TK_INT16:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_S_Z_INCSRC, offset));
	    break;
	case TSF_TK_UINT32:
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_L_Z_INCSRC, offset));
	    break;
	case TSF_TK_UINT64:
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_LL_Z_INCSRC, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_F_Z_INCSRC, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_COPY_NTOHC_TO_D_Z_INCSRC, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_INT16:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_BIT_INCSRC, offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_C_INCSRC, offset));
	    break;
	case TSF_TK_UINT16:
	case TSF_TK_INT16:
	    C(append(proto, GPC_I_COPY_NTOHS_INCSRC, offset));
	    break;
	case TSF_TK_UINT32:
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_L_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT64:
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_LL_E_INCSRC, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_F_E_INCSRC, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_D_E_INCSRC, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_UINT16:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_BIT_INCSRC, offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_C_INCSRC, offset));
	    break;
	case TSF_TK_UINT16:
	case TSF_TK_INT16:
	    C(append(proto, GPC_I_COPY_NTOHS_INCSRC, offset));
	    break;
	case TSF_TK_UINT32:
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_L_Z_INCSRC, offset));
	    break;
	case TSF_TK_UINT64:
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_LL_Z_INCSRC, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_F_Z_INCSRC, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_COPY_NTOHS_TO_D_Z_INCSRC, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_INT32:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_BIT_INCSRC, offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_C_INCSRC, offset));
	    break;
	case TSF_TK_UINT16:
	case TSF_TK_INT16:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_S_INCSRC, offset));
	    break;
	case TSF_TK_UINT32:
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_COPY_NTOHL_INCSRC, offset));
	    break;
	case TSF_TK_UINT64:
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_LL_E_INCSRC, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_F_E_INCSRC, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_D_E_INCSRC, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_UINT32:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_BIT_INCSRC, offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_C_INCSRC, offset));
	    break;
	case TSF_TK_UINT16:
	case TSF_TK_INT16:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_S_INCSRC, offset));
	    break;
	case TSF_TK_UINT32:
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_COPY_NTOHL_INCSRC, offset));
	    break;
	case TSF_TK_UINT64:
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_LL_Z_INCSRC, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_F_Z_INCSRC, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_COPY_NTOHL_TO_D_Z_INCSRC, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_INT64:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_BIT_INCSRC, offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_C_INCSRC, offset));
	    break;
	case TSF_TK_UINT16:
	case TSF_TK_INT16:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_S_INCSRC, offset));
	    break;
	case TSF_TK_UINT32:
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_L_INCSRC, offset));
	    break;
	case TSF_TK_UINT64:
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_COPY_NTOHLL_INCSRC, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_F_E_INCSRC, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_D_E_INCSRC, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_UINT64:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_BIT_INCSRC, offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_C_INCSRC, offset));
	    break;
	case TSF_TK_UINT16:
	case TSF_TK_INT16:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_S_INCSRC, offset));
	    break;
	case TSF_TK_UINT32:
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_L_INCSRC, offset));
	    break;
	case TSF_TK_UINT64:
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_COPY_NTOHLL_INCSRC, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_F_Z_INCSRC, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_COPY_NTOHLL_TO_D_Z_INCSRC, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_FLOAT:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_COPY_NTOHF_TO_BIT_INCSRC, offset));
	    break;
	case TSF_TK_INT8:
	    C(append(proto, GPC_I_COPY_NTOHF_TO_C_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_COPY_NTOHF_TO_C_Z_INCSRC, offset));
	    break;
	case TSF_TK_INT16:
	    C(append(proto, GPC_I_COPY_NTOHF_TO_S_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT16:
	    C(append(proto, GPC_I_COPY_NTOHF_TO_S_Z_INCSRC, offset));
	    break;
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_COPY_NTOHF_TO_L_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT32:
	    C(append(proto, GPC_I_COPY_NTOHF_TO_L_Z_INCSRC, offset));
	    break;
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_COPY_NTOHF_TO_LL_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT64:
	    C(append(proto, GPC_I_COPY_NTOHF_TO_LL_Z_INCSRC, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_COPY_NTOHF_INCSRC, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_COPY_NTOHF_TO_D_INCSRC, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_DOUBLE:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_COPY_NTOHD_TO_BIT_INCSRC, offset));
	    break;
	case TSF_TK_INT8:
	    C(append(proto, GPC_I_COPY_NTOHD_TO_C_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_COPY_NTOHD_TO_C_Z_INCSRC, offset));
	    break;
	case TSF_TK_INT16:
	    C(append(proto, GPC_I_COPY_NTOHD_TO_S_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT16:
	    C(append(proto, GPC_I_COPY_NTOHD_TO_S_Z_INCSRC, offset));
	    break;
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_COPY_NTOHD_TO_L_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT32:
	    C(append(proto, GPC_I_COPY_NTOHD_TO_L_Z_INCSRC, offset));
	    break;
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_COPY_NTOHD_TO_LL_E_INCSRC, offset));
	    break;
	case TSF_TK_UINT64:
	    C(append(proto, GPC_I_COPY_NTOHD_TO_LL_Z_INCSRC, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_COPY_NTOHD_TO_F_INCSRC, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_COPY_NTOHD_INCSRC, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_INTEGER:
        tsf_assert(do_bounds_check);
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_TSF_INTEGER_READ_TO_BIT, offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_TSF_INTEGER_READ_TO_C, offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto, GPC_I_TSF_INTEGER_READ_TO_S, offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_TSF_INTEGER_READ, offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_TSF_INTEGER_READ_TO_LL, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_TSF_INTEGER_READ_TO_F, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_TSF_INTEGER_READ_TO_D, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_LONG:
        tsf_assert(do_bounds_check);
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_TSF_LONG_READ_TO_BIT, offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto, GPC_I_TSF_LONG_READ_TO_C, offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto, GPC_I_TSF_LONG_READ_TO_S, offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_TSF_LONG_READ_TO_L, offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_TSF_LONG_READ, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_TSF_LONG_READ_TO_F, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_TSF_LONG_READ_TO_D, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_BIT:
	switch (dest_type->kind_code) {
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	case TSF_TK_BIT:
	    C(append(proto, GPC_I_BIT_READ, offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto, GPC_I_BIT_READ_TO_S, offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto, GPC_I_BIT_READ_TO_L, offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto, GPC_I_BIT_READ_TO_LL, offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto, GPC_I_BIT_READ_TO_F, offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto, GPC_I_BIT_READ_TO_D, offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_STRUCT:
	if (dest_type->u.s.constructor != NULL) {
	    C(append(proto, GPC_I_CALL,
		     offset,
		     dest_type->u.s.constructor));
	}
	if (dest_type->u.s.pre_destructor != NULL) {
	    C(append(proto, GPC_I_ADD_CBACK,
		     offset,
		     dest_type->u.s.pre_destructor));
	}

	for (i = 0; i < tsf_struct_type_get_num_elements(dest_type); ++i) {
	    tsf_named_type_t *dn = tsf_struct_type_get_element(dest_type, i);
	    if (tsf_struct_type_find_node(src_type, dn->name) == NULL) {
		C(generate_set_default(proto, dn->type, offset + dn->offset));
	    }
	}
	for (i = 0; i < tsf_struct_type_get_num_elements(src_type); ++i) {
	    tsf_named_type_t *sn = tsf_struct_type_get_element(src_type, i);
	    tsf_named_type_t *dn;
	    if (sn->type->kind_code == TSF_TK_BIT) {
		num_bits++;
		continue;
	    }
	    dn = tsf_struct_type_find_node(dest_type, sn->name);
	    if (dn == NULL) {
		/* skip */
		C(generate_parser(proto,
				  tsf_type_create(TSF_TK_VOID),
				  sn->type,
				  0,
				  label,
				  do_bounds_check));
	    } else {
		/* read it */
		C(generate_parser(proto,
				  dn->type,
				  sn->type,
				  offset + dn->offset,
				  label,
				  do_bounds_check));
	    }
	}
            
	if (num_bits) {
	    if (do_bounds_check) {
		C(append(proto,
			 GPC_I_BC,
			 (gpc_cell_t)((num_bits + 7) >> 3)));
	    }
                
	    for (i = 0; i < tsf_struct_type_get_num_elements(src_type); ++i) {
		tsf_named_type_t *sn =
		    tsf_struct_type_get_element(src_type, i);
		tsf_named_type_t *dn;
		if (sn->type->kind_code != TSF_TK_BIT) {
		    continue;
		}
		dn = tsf_struct_type_find_node(dest_type, sn->name);
		if (dn != NULL) {
		    switch (dn->type->kind_code) {
		    case TSF_TK_VOID:
			break;
		    case TSF_TK_BIT:
		    case TSF_TK_INT8:
		    case TSF_TK_UINT8:
			C(append(proto,
				 GPC_I_BIT_MASK_READ,
				 bit,
				 offset + dn->offset));
			break;
		    case TSF_TK_INT16:
		    case TSF_TK_UINT16:
			C(append(proto,
				 GPC_I_BIT_MASK_READ_TO_S,
				 bit,
				 offset + dn->offset));
			break;
		    case TSF_TK_INT32:
		    case TSF_TK_UINT32:
                    case TSF_TK_INTEGER:
			C(append(proto,
				 GPC_I_BIT_MASK_READ_TO_L,
				 bit,
				 offset + dn->offset));
			break;
		    case TSF_TK_INT64:
		    case TSF_TK_UINT64:
                    case TSF_TK_LONG:
			C(append(proto,
				 GPC_I_BIT_MASK_READ_TO_LL,
				 bit,
				 offset + dn->offset));
			break;
		    case TSF_TK_FLOAT:
			C(append(proto,
				 GPC_I_BIT_MASK_READ_TO_F,
				 bit,
				 offset + dn->offset));
			break;
		    case TSF_TK_DOUBLE:
			C(append(proto,
				 GPC_I_BIT_MASK_READ_TO_D,
				 bit,
				 offset + dn->offset));
			break;
		    default:
			tsf_abort("Bad src_type/dest_type combination; "
				  "should have been caught by "
				  "tsf_type_instanceof()");
			break;
		    }
		}
		++bit;
		if (bit == 8) {
		    bit = 0;
		    C(append(proto, GPC_I_SKIP, 1));
		}
	    }
	    if (bit) {
		C(append(proto, GPC_I_SKIP, 1));
	    }
	}
	break;
    case TSF_TK_ARRAY:
	if (dest_type->u.a.element_type->kind_code == TSF_TK_VOID) {
	    C(append(proto,
		     GPC_I_ARRAY_LEN_READ_FIELD,
		     offset + tsf_offsetof(tsf_native_void_array_t,
                                           len)));
                
	    if (src_type->u.a.element_type->kind_code == TSF_TK_BIT) {
		C(append(proto,
			 GPC_I_BITVECTOR_BC_AND_SKIP_FIELD,
			 offset + tsf_offsetof(tsf_native_void_array_t,
                                               len)));
	    } else if (src_type->u.a.element_type->kind_code
		       == TSF_TK_VOID) {
		/* nothing to do */
	    } else {
		static_size =
		    tsf_type_get_static_size(src_type->u.a.element_type);
		if (static_size == UINT32_MAX) {
		    loop_label = label++;
		    done_label = label++;
                        
		    C(append(proto,
			     GPC_I_PUSH_VAL,
			     offset + tsf_offsetof(tsf_native_void_array_t,
                                                   len)));
                        
		    C(append(proto, GPC_I_ZEROJUMP, done_label));
		    C(append(proto, GPC_I_LABEL, loop_label));
		    C(generate_parser(proto,
				      tsf_type_create(TSF_TK_VOID),
				      src_type->u.a.element_type,
				      0,
				      label,
				      tsf_true));
		    C(append(proto, GPC_I_DECCOMPJUMP, loop_label));
		    C(append(proto, GPC_I_LABEL, done_label));
                        
		    C(append(proto, GPC_I_POP));
		} else {
		    C(append(proto,
			     GPC_I_ARRAY_BC_AND_SKIP_FIELD,
			     offset + tsf_offsetof(tsf_native_void_array_t,
                                                   len),
			     static_size));
		}
	    }
	} else if (dest_type->u.a.element_type->kind_code == TSF_TK_BIT) {
	    switch (src_type->u.a.element_type->kind_code) {
	    case TSF_TK_BIT:
		C(append(proto,
			 GPC_I_BITVECTOR_READ,
			 offset));
		break;
	    case TSF_TK_UINT8:
	    case TSF_TK_INT8:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_FROM_C,
			 offset));
		break;
	    case TSF_TK_UINT16:
	    case TSF_TK_INT16:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_FROM_S,
			 offset));
		break;
	    case TSF_TK_UINT32:
	    case TSF_TK_INT32:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_FROM_L,
			 offset));
		break;
	    case TSF_TK_UINT64:
	    case TSF_TK_INT64:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_FROM_LL,
			 offset));
		break;
            case TSF_TK_INTEGER:
                C(append(proto,
                         GPC_I_BITVECTOR_READ_FROM_TSF_INTEGER_ARRAY,
                         offset));
                break;
            case TSF_TK_LONG:
                C(append(proto,
                         GPC_I_BITVECTOR_READ_FROM_TSF_LONG_ARRAY,
                         offset));
                break;
	    case TSF_TK_FLOAT:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_FROM_F,
			 offset));
		break;
	    case TSF_TK_DOUBLE:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_FROM_D,
			 offset));
		break;
	    default:
		tsf_abort("odd case");
		break;
	    }
	} else if (src_type->u.a.element_type->kind_code == TSF_TK_BIT) {
	    switch (dest_type->u.a.element_type->kind_code) {
	    case TSF_TK_UINT8:
	    case TSF_TK_INT8:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_TO_C,
			 offset));
		break;
	    case TSF_TK_UINT16:
	    case TSF_TK_INT16:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_TO_S,
			 offset));
		break;
	    case TSF_TK_UINT32:
	    case TSF_TK_INT32:
            case TSF_TK_INTEGER:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_TO_L,
			 offset));
		break;
	    case TSF_TK_UINT64:
	    case TSF_TK_INT64:
            case TSF_TK_LONG:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_TO_LL,
			 offset));
		break;
	    case TSF_TK_FLOAT:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_TO_F,
			 offset));
		break;
	    case TSF_TK_DOUBLE:
		C(append(proto,
			 GPC_I_BITVECTOR_READ_TO_D,
			 offset));
		break;
	    default:
		tsf_abort("odd case");
		break;
	    }
	} else if (src_type->u.a.element_type->kind_code
		   == dest_type->u.a.element_type->kind_code &&
		   (src_type->u.a.element_type->kind_code == TSF_TK_UINT8 ||
		    src_type->u.a.element_type->kind_code == TSF_TK_INT8
#ifndef NEED_INT_CONVERSION
		    || (tsf_type_kind_is_int(src_type->u.a.element_type->kind_code)
                        && src_type->u.a.element_type->kind_code != TSF_TK_INTEGER
                        && src_type->u.a.element_type->kind_code != TSF_TK_LONG)
#endif
#ifndef NEED_FLOAT_CONVERSION
		    || tsf_type_kind_is_float(src_type->u.a.element_type->kind_code)
#endif
                    )) {
	    C(append(proto,
		     GPC_I_BYTE_ARRAY_READ,
		     offset,
		     tsf_type_get_static_size(src_type->u.a.element_type)));
	} else {
	    C(append(proto,
		     GPC_I_ARRAY_LEN_READ_FIELD,
		     offset + tsf_offsetof(tsf_native_array_t, len)));
                
	    static_size = tsf_type_get_static_size(src_type->u.a.element_type);
	    if (static_size != UINT32_MAX) {
		C(append(proto,
			 GPC_I_ARRAY_BC_FIELD,
			 offset + tsf_offsetof(tsf_native_array_t, len),
			 static_size));
	    }
            
	    C(append(proto,
		     GPC_I_ALLOC_ARRAY,
		     offset,
		     tsf_native_type_get_size(dest_type->u.a.element_type)));
                
	    loop_label = label++;
	    done_label = label++;
                
	    C(append(proto,
		     GPC_I_REPUSH_MULADD_PTR,
		     offset + tsf_offsetof(tsf_native_array_t,len),
		     tsf_native_type_get_size(dest_type->u.a.element_type)));
	    C(append(proto,
		     GPC_I_COMPFAILJUMP,
		     done_label));
	    C(append(proto, GPC_I_LABEL, loop_label));
	    C(generate_parser(proto,
			      dest_type->u.a.element_type,
			      src_type->u.a.element_type,
			      0,
			      label,
			      static_size == UINT32_MAX));
	    C(append(proto,
		     GPC_I_ADDCOMPJUMP,
		     tsf_native_type_get_size(dest_type->u.a.element_type),
		     loop_label));
	    C(append(proto, GPC_I_LABEL, done_label));
	    C(append(proto, GPC_I_TWO_POP));
	}
	break;
    case TSF_TK_CHOICE:
	if (tsf_choice_type_get_num_elements(src_type) >= 256) {
            if (src_type->u.h.choice_as_full_word) {
                if (do_bounds_check) {
                    C(append(proto, GPC_I_BC, 4));
                }
                C(append(proto, GPC_I_READL));
            } else {
                C(append(proto, GPC_I_TSF_UNSIGNED_READ_SUB1));
            }
	} else {
	    if (do_bounds_check) {
		C(append(proto, GPC_I_BC, 1));
	    }
	    C(append(proto, GPC_I_READC_TO_CHOICE));
	}
            
	if (tsf_choice_type_has_non_void(src_type)) {
	    /* use tablejump */
	    label_offset = label;
	    label += tsf_choice_type_get_num_elements(src_type);
	    done_label = label++;
	    really_done_label = label++;
	    targets=malloc(sizeof(gpc_cell_t) *
			   tsf_choice_type_get_num_elements(src_type));
	    if (targets == NULL) {
		tsf_set_errno("Could not malloc array of "
			      "gpc_cell_t");
		return tsf_false;
	    }
	    for (i = 0; i < tsf_choice_type_get_num_elements(src_type); ++i) {
		targets[i] = label_offset + i;
	    }
	    result=append_tablejump_local(proto,
					  tsf_choice_type_get_num_elements(src_type),
					  targets);
	    free(targets);
	    if (!result) {
		return tsf_false;
	    }
	    C(append(proto,
		     GPC_I_SET_L,
		     (gpc_cell_t)(offset + dest_type->u.h.value_offset),
		     (gpc_cell_t)UINT32_MAX));
	    C(append(proto, GPC_I_JUMP, really_done_label));
	    for (i = 0; i < tsf_choice_type_get_num_elements(src_type); ++i) {
		tsf_named_type_t *sn =
		    tsf_choice_type_get_element(src_type, i);
		tsf_named_type_t *dn =
		    tsf_choice_type_find_node(dest_type, sn->name);
		C(append(proto, GPC_I_LABEL, label_offset + i));
		if (dn == NULL) {
		    C(append(proto,
			     GPC_I_SET_L,
			     (gpc_cell_t)offset + dest_type->u.h.value_offset,
			     (gpc_cell_t)UINT32_MAX));
		    C(generate_parser(proto,
				      tsf_type_create(TSF_TK_VOID),
				      sn->type,
				      0,
				      label,
				      tsf_true));
		    C(append(proto, GPC_I_JUMP, really_done_label));
		} else {
		    C(append(proto,
			     GPC_I_SET_L,
			     (gpc_cell_t)offset + dest_type->u.h.value_offset,
			     (gpc_cell_t)dn->index));
		    if (tsf_native_choice_type_is_in_place(dest_type)) {
			C(generate_parser(proto,
					  dn->type,
					  sn->type,
					  offset + dest_type->u.h.data_offset,
					  label,
					  tsf_true));
		    } else {
			C(append(proto,
				 GPC_I_ALLOC,
				 tsf_native_type_get_size(dn->type)));
			C(append(proto,
				 GPC_I_STORE_PTR,
				 offset + dest_type->u.h.data_offset));
			C(generate_parser(proto,
					  dn->type,
					  sn->type,
					  0,
					  label,
					  tsf_true));
		    }
		    C(append(proto, GPC_I_JUMP, done_label));
		}
	    }
	    C(append(proto, GPC_I_LABEL, done_label));
	    if (!tsf_native_choice_type_is_in_place(dest_type)) {
		C(append(proto, GPC_I_POP));
	    }
	    C(append(proto, GPC_I_LABEL, really_done_label));
	} else if (tsf_choice_type_uses_same_indices(src_type,
						     dest_type)) {
	    /* use checkchoice and store */
	    C(append(proto,
		     GPC_I_CHECKCHOICE,
		     tsf_choice_type_get_num_elements(src_type)));
	    C(append(proto,
		     GPC_I_STORE_VAL,
		     offset + dest_type->u.h.value_offset));
	    C(append(proto, GPC_I_POP));
	} else {
	    /* use tableset_local_to_field */
	    targets=malloc(sizeof(gpc_cell_t) *
			   tsf_choice_type_get_num_elements(src_type));
	    if (targets==NULL) {
		tsf_set_errno("Could not malloc array of "
			      "gpc_cell_t");
		return tsf_false;
	    }
	    for (i = 0; i < tsf_choice_type_get_num_elements(src_type); ++i) {
		tsf_named_type_t *sn =
		    tsf_choice_type_get_element(src_type, i);
		tsf_named_type_t *dn =
		    tsf_choice_type_find_node(dest_type, sn->name);
		if (dn == NULL) {
		    targets[i] = UINT32_MAX;
		} else {
		    targets[i] = dn->index;
		}
	    }
	    result = append_tableset_local_to_field(
                proto,
                offset + dest_type->u.h.value_offset,
                tsf_choice_type_get_num_elements(src_type),
                targets);
	    free(targets);
	    if (!result) {
		return tsf_false;
	    }
	}
	break;
    case TSF_TK_STRING:
	C(append(proto, GPC_I_STRING_READ, offset));
	break;
    case TSF_TK_ANY:
	C(append(proto, GPC_I_ANY_READ, offset));
	break;
    default:
	tsf_abort("Unknown type");
	break;
    }
    
    return tsf_true;
}

gpc_proto_t *tsf_gpc_code_gen_generate_parser(tsf_type_t *dest_type,
                                              tsf_type_t *src_type) {
    gpc_proto_t *proto;

    if (!tsf_native_type_has_struct_mapping(dest_type)) {
        tsf_set_error(TSF_E_NO_STRUCT_MAP, NULL);
        return NULL;
    }
    
    if (!tsf_type_instanceof(src_type, dest_type)) {
        tsf_set_error(TSF_E_TYPE_MISMATCH, NULL);
        return NULL;
    }
    
    proto=gpc_proto_create(2);
    if (proto==NULL) {
        return NULL;
    }
    
    if (!append(proto, GPC_I_POP_BUF)) {
        gpc_proto_destroy(proto);
        return NULL;
    }

    if (!generate_parser(proto,
                         dest_type,
                         src_type,
                         0,
                         0,
                         tsf_true)) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    if (!append(proto, GPC_I_RETURN_ONE)) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    return proto;
}

static tsf_bool_t generate_converter(gpc_proto_t *proto,
                                     tsf_type_t *dest_type,
                                     tsf_type_t *src_type,
                                     gpc_cell_t dest_offset,
                                     gpc_cell_t src_offset,
                                     gpc_cell_t label) {
    uint32_t i;
    gpc_cell_t done_label,really_done_label;
    gpc_cell_t loop_label;
    gpc_cell_t label_offset;
    gpc_cell_t *targets;
    tsf_bool_t result;
    
    if (dest_type->kind_code == TSF_TK_VOID) {
        return tsf_true;
    }
    
    if (!tsf_type_instanceof(src_type, dest_type)) {
	C(generate_set_default_copier(proto, dest_type, dest_offset));
    }
    
    /* reason why we don't memcpy primitives: if we see a primitive at
       this point, then it is just one word - so a primitive copy
       operation will be faster than memcpy.
       
       interestingly, if we see a struct that contains exactly one
       primitive, we will still use memcpy (unless it is a byte).  this
       is not optimal, but oh well. */
    if (!tsf_type_kind_is_primitive(tsf_type_get_kind_code(src_type)) &&
        tsf_native_type_can_blit(dest_type,src_type)) {
        switch (tsf_native_type_get_size(src_type)) {
	case 0:
	    break;
	case 1:
	    C(append(proto,
		     GPC_I_COPY_C,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    C(append(proto,
		     GPC_I_MEMCPY,
		     dest_offset,
		     src_offset,
		     tsf_native_type_get_size(src_type)));
	    break;
        }
        return tsf_true;
    }
    
    switch (src_type->kind_code) {
    case TSF_TK_BIT:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    tsf_abort("bit-to-bit copies should have been handled elsewhere");
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_BIT_TO_S,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_BIT_TO_L,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_BIT_TO_LL,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_BIT_TO_F,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_BIT_TO_D,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_INT8:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto,
		     GPC_I_COPY_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_C,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_C_TO_S_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_C_TO_L_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_C_TO_LL_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_C_TO_F_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_C_TO_D_E,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_UINT8:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto,
		     GPC_I_COPY_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_C,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_C_TO_S_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_C_TO_L_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_C_TO_LL_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_C_TO_F_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_C_TO_D_Z,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_INT16:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto,
		     GPC_I_COPY_S_TO_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_S_TO_C,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_S,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_S_TO_L_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_S_TO_LL_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_S_TO_F_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_S_TO_D_E,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_UINT16:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto,
		     GPC_I_COPY_S_TO_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_S_TO_C,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_S,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_S_TO_L_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_S_TO_LL_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_S_TO_F_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_S_TO_D_Z,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_INT32:
    case TSF_TK_INTEGER:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto,
		     GPC_I_COPY_L_TO_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_L_TO_C,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_L_TO_S,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_L,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_L_TO_LL_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_L_TO_F_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_L_TO_D_E,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_UINT32:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto,
		     GPC_I_COPY_L_TO_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_L_TO_C,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_L_TO_S,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_L,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_L_TO_LL_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_L_TO_F_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_L_TO_D_Z,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_INT64:
    case TSF_TK_LONG:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_C,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_S,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_L,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_LL,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_F_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_D_E,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_UINT64:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT8:
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_C,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_S,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
	case TSF_TK_UINT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_L,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
	case TSF_TK_UINT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_LL,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_F_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_LL_TO_D_Z,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_FLOAT:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto,
		     GPC_I_COPY_F_TO_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_F_TO_C_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT8:
	    C(append(proto,
		     GPC_I_COPY_F_TO_C_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_F_TO_S_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	    C(append(proto,
		     GPC_I_COPY_F_TO_S_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_UINT32:
	    C(append(proto,
		     GPC_I_COPY_F_TO_L_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_F_TO_L_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_UINT64:
	    C(append(proto,
		     GPC_I_COPY_F_TO_LL_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_F_TO_LL_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_F,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_F_TO_D,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_DOUBLE:
	switch (dest_type->kind_code) {
	case TSF_TK_BIT:
	    C(append(proto,
		     GPC_I_COPY_D_TO_BIT,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_UINT8:
	    C(append(proto,
		     GPC_I_COPY_D_TO_C_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT8:
	    C(append(proto,
		     GPC_I_COPY_D_TO_C_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_UINT16:
	    C(append(proto,
		     GPC_I_COPY_D_TO_S_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT16:
	    C(append(proto,
		     GPC_I_COPY_D_TO_S_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_UINT32:
	    C(append(proto,
		     GPC_I_COPY_D_TO_L_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT32:
        case TSF_TK_INTEGER:
	    C(append(proto,
		     GPC_I_COPY_D_TO_L_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_UINT64:
	    C(append(proto,
		     GPC_I_COPY_D_TO_LL_Z,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_INT64:
        case TSF_TK_LONG:
	    C(append(proto,
		     GPC_I_COPY_D_TO_LL_E,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_FLOAT:
	    C(append(proto,
		     GPC_I_COPY_D_TO_F,
		     dest_offset,
		     src_offset));
	    break;
	case TSF_TK_DOUBLE:
	    C(append(proto,
		     GPC_I_COPY_D,
		     dest_offset,
		     src_offset));
	    break;
	default:
	    tsf_abort("Bad src_type/dest_type combination; "
		      "should have been caught by "
		      "tsf_type_instanceof()");
	    break;
	}
	break;
    case TSF_TK_STRUCT:
        if (dest_type->u.s.constructor != NULL ||
            dest_type->u.s.pre_destructor != NULL) {
            /* This could be optimized by adding I_CALL_2ND and I_ADD_CBACK_2ND. I don't think
               it's worth it. */
            
            C(append(proto, GPC_I_DUP_PTR_2ND));
            if (dest_type->u.s.constructor != NULL) {
                C(append(proto, GPC_I_CALL,
                         dest_offset,
                         dest_type->u.s.constructor));
            }
            if (dest_type->u.s.pre_destructor != NULL) {
                C(append(proto, GPC_I_ADD_CBACK,
                         dest_offset,
                         dest_type->u.s.pre_destructor));
            }
            C(append(proto, GPC_I_POP));
        }

	for (i = 0; i < tsf_struct_type_get_num_elements(dest_type); ++i) {
	    tsf_named_type_t *n =
		tsf_struct_type_get_element(dest_type, i);
                
	    tsf_named_type_t *n2 =
		tsf_struct_type_find_node(src_type, n->name);
	    
	    if (n2 == NULL) {
		C(generate_set_default_copier(
                      proto, n->type, dest_offset + n->offset));
	    } else {
		C(generate_converter(proto,
				     n->type,
				     n2->type,
				     dest_offset + n->offset,
				     src_offset + n2->offset,
				     label));
	    }
	}
	break;
    case TSF_TK_ARRAY:
	if (dest_type->u.a.element_type->kind_code == TSF_TK_VOID) {
	    C(append(proto,
		     GPC_I_COPY_L,
		     (gpc_cell_t)(dest_offset +
                                  tsf_offsetof(tsf_native_void_array_t, len)),
		     (gpc_cell_t)(src_offset +
                                  src_type->u.a.element_type->kind_code == TSF_TK_VOID
                                  ? tsf_offsetof(tsf_native_void_array_t, len)
                                  : (src_type->u.a.element_type->kind_code == TSF_TK_BIT
                                     ? tsf_offsetof(tsf_native_bitvector_t, num_bits)
                                     : tsf_offsetof(tsf_native_array_t, len)))));
	} else if (dest_type->u.a.element_type->kind_code == TSF_TK_BIT) {
	    switch (src_type->u.a.element_type->kind_code) {
	    case TSF_TK_BIT:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_UINT8:
	    case TSF_TK_INT8:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_FROM_C,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_UINT16:
	    case TSF_TK_INT16:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_FROM_S,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_UINT32:
	    case TSF_TK_INT32:
            case TSF_TK_INTEGER:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_FROM_L,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_UINT64:
	    case TSF_TK_INT64:
            case TSF_TK_LONG:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_FROM_LL,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_FLOAT:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_FROM_F,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_DOUBLE:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_FROM_D,
			 dest_offset,
			 src_offset));
		break;
	    default:
		tsf_abort("case missed");
		break;
	    }
	} else if (src_type->u.a.element_type->kind_code == TSF_TK_BIT) {
	    switch (dest_type->u.a.element_type->kind_code) {
	    case TSF_TK_UINT8:
	    case TSF_TK_INT8:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_TO_C,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_UINT16:
	    case TSF_TK_INT16:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_TO_S,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_UINT32:
	    case TSF_TK_INT32:
            case TSF_TK_INTEGER:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_TO_L,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_UINT64:
	    case TSF_TK_INT64:
            case TSF_TK_LONG:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_TO_LL,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_FLOAT:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_TO_F,
			 dest_offset,
			 src_offset));
		break;
	    case TSF_TK_DOUBLE:
		C(append(proto,
			 GPC_I_COPY_BITVECTOR_TO_D,
			 dest_offset,
			 src_offset));
		break;
	    default:
		tsf_abort("case missed");
	    }
	} else if (tsf_native_type_can_blit(dest_type->u.a.element_type,
					    src_type->u.a.element_type) &&
		   tsf_native_type_get_size(dest_type->u.a.element_type) ==
		   tsf_native_type_get_size(src_type->u.a.element_type)) {
	    C(append(proto,
		     GPC_I_COPY_ARRAY,
		     dest_offset,
		     src_offset,
		     tsf_native_type_get_size(dest_type->u.a.element_type)));
	} else {
	    loop_label = label++;
	    done_label = label++;
                
	    C(append(proto,
		     GPC_I_COPY_L,
		     dest_offset + tsf_offsetof(tsf_native_array_t, len),
		     src_offset + tsf_offsetof(tsf_native_array_t, len)));
	    C(append(proto,
		     GPC_I_ALLOC_ARRAY_2ND,
		     dest_offset,
		     tsf_native_type_get_size(dest_type->u.a.element_type)));
	    C(append(proto,
		     GPC_I_REPUSH_MULADD_PTR_2ND,
		     dest_offset + tsf_offsetof(tsf_native_array_t, len),
		     tsf_native_type_get_size(dest_type->u.a.element_type)));
	    C(append(proto,
		     GPC_I_PUSH_PTR_3RD,
		     src_offset + tsf_offsetof(tsf_native_array_t, data)));
	    C(append(proto,
		     GPC_I_COMPFAILJUMP_2ND,
		     done_label));
	    C(append(proto,GPC_I_LABEL, loop_label));
	    C(generate_converter(proto,
				 dest_type->u.a.element_type,
				 src_type->u.a.element_type,
				 0,
				 0,
				 label));
	    C(append(proto,
		     GPC_I_TWO_ADDCOMPJUMP,
		     tsf_native_type_get_size(dest_type->u.a.element_type),
		     tsf_native_type_get_size(src_type->u.a.element_type),
		     loop_label));
	    C(append(proto, GPC_I_LABEL, done_label));
	    C(append(proto, GPC_I_THREE_POP));
	}
	break;
    case TSF_TK_CHOICE:
	targets=malloc(sizeof(gpc_cell_t) *
		       tsf_choice_type_get_num_elements(src_type));
	if (targets == NULL) {
	    tsf_set_errno("Could not malloc array of "
			  "gpc_cell_t");
	    return tsf_false;
	}
	if (tsf_choice_type_has_non_void(src_type)) {
	    label_offset = label;
	    label += tsf_choice_type_get_num_elements(src_type);
	    done_label = label++;
	    really_done_label = label++;
	    for (i = 0; i < tsf_choice_type_get_num_elements(src_type); ++i) {
		targets[i] = label_offset + i;
	    }
	    result = append_tablejump_field(
                proto,
                src_offset + src_type->u.h.value_offset,
                tsf_choice_type_get_num_elements(src_type),
                targets);
	    free(targets);
	    if (!result) {
		return tsf_false;
	    }
	    C(append(proto,
		     GPC_I_SET_2ND_L,
		     (gpc_cell_t)(dest_offset + dest_type->u.h.value_offset),
		     (gpc_cell_t)UINT32_MAX));
	    C(append(proto, GPC_I_JUMP, really_done_label));
	    for (i = 0; i < tsf_choice_type_get_num_elements(src_type); ++i) {
		tsf_named_type_t *sn =
		    tsf_choice_type_get_element(src_type, i);
		tsf_named_type_t *dn =
		    tsf_choice_type_find_node(dest_type, sn->name);
		C(append(proto, GPC_I_LABEL, label_offset + i));
		if (dn == NULL) {
		    C(append(proto,
			     GPC_I_SET_2ND_L,
			     (gpc_cell_t)(dest_offset + dest_type->u.h.value_offset),
			     (gpc_cell_t)UINT32_MAX));
		    C(append(proto, GPC_I_JUMP, really_done_label));
		} else {
		    C(append(proto,
			     GPC_I_SET_2ND_L,
			     (gpc_cell_t)(dest_offset + dest_type->u.h.value_offset),
			     (gpc_cell_t)dn->index));
		    if (!tsf_native_choice_type_is_in_place(src_type) ||
			!tsf_native_choice_type_is_in_place(dest_type)) {
			uint32_t src_off = 0;
			uint32_t dest_off = 0;
			if (tsf_native_choice_type_is_in_place(dest_type)) {
			    C(append(proto, GPC_I_DUP_PTR_2ND));
			    dest_off = dest_offset +
				dest_type->u.h.data_offset;
			} else {
			    /* must alloc */
			    C(append(proto, GPC_I_ALLOC,
				     tsf_native_type_get_size(dn->type)));
			    C(append(proto, GPC_I_STORE_PTR_2ND,
				     dest_offset +
				     dest_type->u.h.data_offset));
			}
			if (tsf_native_choice_type_is_in_place(src_type)) {
			    C(append(proto, GPC_I_DUP_PTR_2ND));
			    src_off = src_offset +
				src_type->u.h.data_offset;
			} else {
			    /* just load */
			    C(append(proto, GPC_I_PUSH_PTR_2ND,
				     src_offset +
				     src_type->u.h.data_offset));
			}
			C(generate_converter(proto,
					     dn->type,
					     sn->type,
					     dest_off,
					     src_off,
					     label));
			C(append(proto, GPC_I_JUMP, done_label));
		    } else {
			C(generate_converter(proto,
					     dn->type,
					     sn->type,
					     dest_offset +
					     dest_type->u.h.data_offset,
					     src_offset +
					     src_type->u.h.data_offset,
					     label));
			C(append(proto, GPC_I_JUMP, really_done_label));
		    }
		}
	    }
	    C(append(proto, GPC_I_LABEL, done_label));
	    if (!tsf_native_choice_type_is_in_place(src_type) ||
		!tsf_native_choice_type_is_in_place(dest_type)) {
		C(append(proto, GPC_I_TWO_POP));
	    }
	    C(append(proto, GPC_I_LABEL, really_done_label));
	} else {
	    for (i = 0; i < tsf_choice_type_get_num_elements(src_type); ++i) {
		tsf_named_type_t *sn =
		    tsf_choice_type_get_element(src_type, i);
		tsf_named_type_t *dn =
		    tsf_choice_type_find_node(dest_type, sn->name);
		if (dn == NULL) {
		    targets[i] = UINT32_MAX;
		} else {
		    targets[i] = dn->index;
		}
	    }
	    result = append_tableset_field_to_field(
                proto,
                dest_offset + dest_type->u.h.value_offset,
                src_offset + src_type->u.h.value_offset,
                tsf_choice_type_get_num_elements(src_type),
                targets);
	    free(targets);
	    if (!result) {
		return tsf_false;
	    }
	}
	break;
    case TSF_TK_STRING:
	C(append(proto,
		 GPC_I_STRING_COPY,
		 dest_offset,
		 src_offset));
	break;
    case TSF_TK_ANY:
	C(append(proto,
		 GPC_I_ANY_COPY,
		 dest_offset,
		 src_offset));
	break;
    default:
	tsf_abort("Bad type.");
	break;
    }

    return tsf_true;
}

gpc_proto_t *tsf_gpc_code_gen_generate_converter(tsf_type_t *dest_type,
                                                 tsf_type_t *src_type) {
    gpc_proto_t *proto;

    if (!tsf_native_type_has_struct_mapping(dest_type)) {
        tsf_set_error(TSF_E_NO_STRUCT_MAP,
                      "Destination lacks a structure mapping");
        return NULL;
    }
    
    if (!tsf_native_type_has_struct_mapping(src_type)) {
        tsf_set_error(TSF_E_NO_STRUCT_MAP,
                      "Source lacks a structure mapping");
        return NULL;
    }
    
    if (!tsf_type_instanceof(src_type, dest_type)) {
        tsf_set_error(TSF_E_TYPE_MISMATCH, NULL);
        return NULL;
    }
    
    proto = gpc_proto_create(2);
    if (proto == NULL) {
        return NULL;
    }
    
    /* FIXME: The caller should allocate the region for us. */
    if (!append(proto,
                GPC_I_ALLOC_ROOT_MAYBE_2ND,
                tsf_native_type_get_size(dest_type))) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    if (!generate_converter(proto,
                            dest_type,
                            src_type,
                            0,
                            0,
                            0)) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    if (!append(proto, GPC_I_POP)) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    if (!append(proto, GPC_I_RETURN_TOP_WITH_REGION)) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    return proto;
}

static tsf_bool_t needs_destruction(tsf_type_t *type) {
    uint32_t i;
    
    switch (type->kind_code) {
    case TSF_TK_STRUCT:
        if (type->u.s.destructor != NULL) {
            return tsf_true;
        } else {
            if (type->u.s.pre_destructor != NULL) {
                return tsf_true;
            }
            for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
                tsf_named_type_t *n = tsf_struct_type_get_element(type, i);
                if (n->type->kind_code == TSF_TK_BIT) {
                    continue;
                }
                if (needs_destruction(n->type)) {
                    return tsf_true;
                }
            }
        }
        break;
    case TSF_TK_ARRAY:
        if (type->u.a.element_type->kind_code == TSF_TK_VOID) {
            /* Nothing to do - a void array isn't really a container. */
        } else {
            return tsf_true;
        }
        break;
    case TSF_TK_CHOICE:
        if (tsf_choice_type_has_non_void(type)) {
            return tsf_true;
        }
        break;
    case TSF_TK_STRING:
        return tsf_true;
    case TSF_TK_ANY:
        return tsf_true;
    default:
        /* None of the other types have things that need to be freed. */
        break;
    }
    
    return tsf_false;
}

static tsf_bool_t generate_destroyer(gpc_proto_t *proto,
                                     tsf_type_t *type,
                                     gpc_cell_t offset,
                                     gpc_cell_t label) {
    uint32_t i;
    gpc_cell_t label_offset;
    gpc_cell_t done_label, really_done_label;
    gpc_cell_t *targets;
    tsf_bool_t result;
    
    switch (type->kind_code) {
    case TSF_TK_STRUCT:
        /* If the struct has a destructor, assume that it will destroy all of the fields,
           including the ones that we know about. */
        if (type->u.s.destructor != NULL) {
            C(append(proto, GPC_I_CALL,
                     offset,
                     type->u.s.destructor));
        } else {
            if (type->u.s.pre_destructor != NULL) {
                C(append(proto, GPC_I_CALL,
                         offset,
                         type->u.s.pre_destructor));
            }
            for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
                tsf_named_type_t *n = tsf_struct_type_get_element(type, i);
                if (n->type->kind_code == TSF_TK_BIT) {
                    continue;
                }
                C(generate_destroyer(proto,
                                     n->type,
                                     offset + n->offset,
                                     label));
            }
        }
        break;
    case TSF_TK_ARRAY:
        if (type->u.a.element_type->kind_code == TSF_TK_VOID) {
            /* Nothing to do - a void array isn't really a container. */
        } else if (type->u.a.element_type->kind_code == TSF_TK_BIT) {
            C(append(proto,
                     GPC_I_FREE,
                     offset + tsf_offsetof(tsf_native_bitvector_t, bits)));
        } else {
            if (needs_destruction(type->u.a.element_type)) {
                gpc_cell_t loop_label = label++;
                done_label = label++;
                C(append(proto,
                         GPC_I_PUSH_PTR,
                         offset + tsf_offsetof(tsf_native_array_t, data)));
                C(append(proto,
                         GPC_I_REPUSH_MULADD_PTR,
                         offset + tsf_offsetof(tsf_native_array_t, len),
                         tsf_native_type_get_size(type->u.a.element_type)));
                C(append(proto, GPC_I_COMPFAILJUMP, done_label));
                C(append(proto, GPC_I_LABEL, loop_label));
                C(generate_destroyer(proto, type->u.a.element_type, 0, label));
                C(append(proto,
                         GPC_I_ADDCOMPJUMP,
                         tsf_native_type_get_size(type->u.a.element_type),
                         loop_label));
                C(append(proto, GPC_I_LABEL, done_label));
                C(append(proto, GPC_I_TWO_POP));
            }
            
            C(append(proto, GPC_I_FREE, offset + tsf_offsetof(tsf_native_array_t, data)));
        }
        break;
    case TSF_TK_CHOICE:
        if (tsf_choice_type_has_non_void(type)) {
            label_offset = label;
            label += tsf_choice_type_get_num_elements(type);
            done_label = label++;
            really_done_label = label++;
            targets=malloc(sizeof(gpc_cell_t)*
                           tsf_choice_type_get_num_elements(type));
            if (targets == NULL) {
                tsf_set_errno("Could not malloc array of "
                              "gpc_cell_t");
                return tsf_false;
            }
            for (i = 0; i < tsf_choice_type_get_num_elements(type); ++i) {
                targets[i] = label_offset + i;
            }
            result=append_tablejump_field(
                proto,
                offset + type->u.h.value_offset,
                tsf_choice_type_get_num_elements(type),
                targets);
            free(targets);
            if (!result) {
                return tsf_false;
            }
            C(append(proto, GPC_I_JUMP, really_done_label)); /* for unknown values */
            for (i = 0; i < tsf_choice_type_get_num_elements(type); ++i) {
                tsf_named_type_t *n = tsf_choice_type_get_element(type, i);
                C(append(proto, GPC_I_LABEL, label_offset + i));
                if (tsf_native_choice_type_is_in_place(type)) {
                    C(generate_destroyer(proto,
                                         n->type,
                                         offset + type->u.h.data_offset,
                                         label));
                } else {
                    /* cannot move this push above the tablejump since the
                       tablejump uses the value at the top of the stack, and
                       I don't feel like adding a second tablejump just to
                       get around that. */
                    C(append(proto, GPC_I_PUSH_PTR,
                             offset + type->u.h.data_offset));
                    C(generate_destroyer(proto,
                                         n->type,
                                         0,
                                         label));
                }
                C(append(proto, GPC_I_JUMP, done_label));
            }
            /* if it is in place, you get two labels...  oh well. */
            C(append(proto, GPC_I_LABEL, done_label));
            if (!tsf_native_choice_type_is_in_place(type)) {
                C(append(proto, GPC_I_FREE_IMMEDIATE));
                C(append(proto, GPC_I_POP));
            }
            C(append(proto, GPC_I_LABEL, really_done_label));
        }
        break;
    case TSF_TK_STRING:
        C(append(proto, GPC_I_FREE, offset));
        break;
    case TSF_TK_ANY:
        C(append(proto, GPC_I_DESTROY_BUFFER, offset));
        break;
    default:
        /* None of the other types have things that need to be freed. */
        break;
    }
    
    return tsf_true;
}

gpc_proto_t *tsf_gpc_code_gen_generate_destroyer(tsf_type_t *type) {
    gpc_proto_t *proto;
    
    if (!tsf_native_type_has_struct_mapping(type)) {
        tsf_set_error(TSF_E_NO_STRUCT_MAP, NULL);
        return NULL;
    }
    
    proto = gpc_proto_create(1);
    if (proto == NULL) {
        return NULL;
    }
    
    if (!generate_destroyer(proto, type, 0, 0)) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    if (!append(proto, GPC_I_RETURN_ONE)) {
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    return proto;
}


