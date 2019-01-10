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
#include "tsf_format.h"

struct addr_array {
    gpc_cell_t **addrs;
    uint32_t num;
};

static tsf_bool_t st_aa_append(tsf_st_table *table,
                               gpc_cell_t key,
                               gpc_cell_t *addr) {
    struct addr_array *aa;
    if (tsf_st_lookup(table,(char*)key,(void**)&aa)) {
        gpc_cell_t **new_array=
            realloc(aa->addrs,sizeof(gpc_cell_t*)*(aa->num+1));
        if (new_array==NULL) {
            tsf_set_errno("Could not realloc gpc_cell_t* array");
            return tsf_false;
        }
        aa->addrs=new_array;
    } else {
        aa=malloc(sizeof(struct addr_array));
        if (aa==NULL) {
            tsf_set_errno("Could not malloc struct addr_array");
            return tsf_false;
        }
        aa->addrs=malloc(sizeof(gpc_cell_t*));
        if (aa->addrs==NULL) {
            tsf_set_errno("Could not malloc gpc_cell_t*");
            free(aa);
            return tsf_false;
        }
        aa->num=0;
        
        if (tsf_st_insert(table,(char*)key,aa)<0) {
            tsf_set_errno("tsf_st_insert failed");
            free(aa->addrs);
            free(aa);
            return tsf_false;
        }
    }
    aa->addrs[aa->num++]=addr;
    return tsf_true;
}

static int st_aa_free(void *key /* we ignored it anyway */,
                      struct addr_array *aa,
                      void *arg) {
    free(aa->addrs);
    free(aa);
    return TSF_ST_CONTINUE;
}

static tsf_bool_t back_branch_action(gpc_cell_t *cell,
                                     void *arg) {
    tsf_st_table *back_labels=arg;
    gpc_cell_t addr;
    if (!tsf_st_lookup(back_labels,
                       (char*)*cell,
                       (void*)&addr)) {
        tsf_set_error(TSF_E_BAD_JUMP,
                      "Backward reference to unknown label: " fuptr,
                      *cell);
        return tsf_false;
    }
    *cell=addr;
    return tsf_true;
}

static tsf_bool_t fore_branch_action(gpc_cell_t *cell,
                                     void *arg) {
    tsf_st_table *fore_labels=arg;
    return st_aa_append(fore_labels,*cell,cell);
}

gpc_intable_t *gpc_intable_from_proto(gpc_proto_t *proto) {
    gpc_intable_t *ret;
    gpc_cell_t *write;
    gpc_cell_t *read;
    tsf_st_table *back_labels,*fore_labels;
    
    ret=malloc(sizeof(gpc_intable_t));
    if (ret==NULL) {
        tsf_set_errno("Could not malloc gpc_intable_t");
        return NULL;
    }
    
    ret->num_args=proto->num_args;
    
    ret->targets=tsf_st_init_ptrtable();
    if (ret->targets==NULL) {
        tsf_set_errno("Could not tsf_st_init_ptrtable()");
        free(ret);
        return NULL;
    }
    
    ret->stream=malloc(sizeof(gpc_cell_t)*proto->size);
    if (ret->stream==NULL) {
        tsf_set_errno("Could not malloc stream for gpc_intable_t");
        tsf_st_free_table(ret->targets);
        free(ret);
        return NULL;
    }
    
    back_labels=tsf_st_init_ptrtable();
    if (back_labels==NULL) {
        tsf_set_errno("Could not tsf_st_init_ptrtable()");
        tsf_st_free_table(ret->targets);
        free(ret->stream);
        free(ret);
        return NULL;
    }
    
    fore_labels=tsf_st_init_ptrtable();
    if (fore_labels==NULL) {
        tsf_set_errno("Could not tsf_st_init_ptrtable()");
        tsf_st_free_table(ret->targets);
        free(ret->stream);
        free(ret);
        tsf_st_free_table(back_labels);
        return NULL;
    }
    
    write=ret->stream;
    
    for (read=proto->stream;read<proto->stream+proto->size;) {
        if (*read==GPC_I_LABEL) {
            uint32_t i;
            struct addr_array *aa;
            gpc_cell_t *to_delete;
            
            if (tsf_st_insert(back_labels,
                              (char*)read[1],
                              (void*)write)<0) {
                tsf_set_errno("Could not tsf_st_insert");
                goto failure;
            }
            
            to_delete = &read[1];
            if (tsf_st_delete(fore_labels, (char**)to_delete, (void**)&aa)) {
                for (i=0;i<aa->num;++i) {
                    *(aa->addrs[i])=(gpc_cell_t)write;
                }
                free(aa->addrs);
                free(aa);
            }
            
            if (tsf_st_insert(ret->targets,
                              (char*)write,
                              NULL)<0) {
                tsf_set_errno("Could not tsf_st_insert");
                goto failure;
            }
            
            read+=2;
        } else {
            uint32_t num;
            gpc_cell_t *pre_write=write;
            
            num=gpc_instruction_size(read);
            *write++=*read++;
            while (--num>0) {
                *write++=*read++;
            }
            
            if (!gpc_instruction_for_all_back_branches(
                     pre_write,back_branch_action,back_labels)) {
                goto failure;
            }
            
            if (!gpc_instruction_for_all_forward_branches(
                     pre_write,fore_branch_action,fore_labels)) {
                goto failure;
            }
        }
    }
    
    if (fore_labels->num_entries!=0) {
        tsf_set_error(TSF_E_BAD_JUMP,
                      "There still exist unfulfilled label references");
        goto failure;
    }
    
    ret->stack_heights=
	gpc_intable_get_stack_heights(ret,&(ret->max_height));
    if (ret->stack_heights==NULL) {
	goto failure;
    }
    
    tsf_st_free_table(back_labels);
    tsf_st_free_table(fore_labels);
    
    ret->size=write-ret->stream;
    
    return ret;
    
failure:
    tsf_st_free_table(ret->targets);
    free(ret->stream);
    free(ret);
    tsf_st_free_table(back_labels);
    tsf_st_foreach(fore_labels,(tsf_st_func_t)st_aa_free,NULL);
    tsf_st_free_table(fore_labels);
    return NULL;
}

struct correct_branch_closure {
    size_t offset;
    tsf_st_table *targets;
};

static tsf_bool_t correct_branch(gpc_cell_t *cell,
                                 void *arg) {
    struct correct_branch_closure *closure=arg;
    *cell=(gpc_cell_t)(((gpc_cell_t*)*cell)+closure->offset);
    return tsf_st_insert(closure->targets,(char*)cell,NULL)>=0;
}

gpc_intable_t *gpc_intable_clone(gpc_intable_t *intable) {
    gpc_intable_t *ret=malloc(sizeof(gpc_intable_t));
    gpc_cell_t *cur;
    struct correct_branch_closure closure;

    if (ret==NULL) {
        tsf_set_errno("Could not malloc gpc_intable_t");
        return NULL;
    }
    
    ret->num_args=intable->num_args;
    ret->size=intable->size;
    
    ret->stream=malloc(sizeof(gpc_cell_t)*ret->size);
    if (ret->stream==NULL) {
        tsf_set_errno("Could not malloc array of gpc_cell_t");
        free(ret);
        return NULL;
    }
    
    memcpy(ret->stream,intable->stream,ret->size*sizeof(gpc_cell_t));
    
    ret->targets=tsf_st_init_ptrtable();
    if (ret->targets==NULL) {
        tsf_set_errno("Could not tsf_st_init_ptrtable()");
        free(ret->stream);
        free(ret);
        return NULL;
    }
    
    closure.offset=ret->stream-intable->stream;
    closure.targets=ret->targets;
    
    for (cur=ret->stream;
         cur<ret->stream+ret->size;
         cur+=gpc_instruction_size(cur)) {
        if (!gpc_instruction_for_all_branches(cur,correct_branch,&closure)) {
            tsf_set_errno("Could not tsf_st_insert");
            tsf_st_free_table(ret->targets);
            free(ret->stream);
            free(ret);
            return NULL;
        }
    }

    ret->stack_heights=
	gpc_intable_get_stack_heights(ret,&(ret->max_height));
    if (ret->stack_heights==NULL) {
	tsf_st_free_table(ret->targets);
	free(ret->stream);
	free(ret);
	return NULL;
    }
    
    return ret;
}

void gpc_intable_destroy(gpc_intable_t *intable) {
    tsf_st_free_table(intable->targets);
    tsf_st_free_table(intable->stack_heights);
    free(intable->stream);
    free(intable);
}

static tsf_bool_t canonicalize_branch(gpc_cell_t *cell,
				      void *arg) {
    gpc_cell_t *begin=arg;
    *cell=((gpc_cell_t*)*cell)-begin;
    return tsf_true;
}

tsf_bool_t gpc_intable_write(gpc_intable_t *intable,
			     tsf_writer_t writer,
			     void *arg) {
    gpc_cell_t *cur;
    for (cur=intable->stream;
	 cur<intable->stream+intable->size;
	 cur+=gpc_instruction_size(cur)) {
	size_t size=sizeof(gpc_cell_t)
	    * gpc_instruction_size(cur);
	gpc_cell_t *buffer=malloc(size);
	tsf_bool_t res;
	if (buffer==NULL) {
	    tsf_set_errno("Could not allocate buffer");
	    return tsf_false;
	}
	memcpy(buffer,cur,size);
	gpc_instruction_for_all_branches(buffer,
					 canonicalize_branch,
					 intable->stream);
	res=writer(arg,buffer,size);
	free(buffer);
	if (!res) {
	    return tsf_false;
	}
    }
    return tsf_true;
}

intptr_t gpc_intable_get_height(gpc_intable_t *prog,
				gpc_cell_t *address) {
    intptr_t result;
    if (!tsf_st_lookup(prog->stack_heights,
		       (char*)address,
		       (void*)&result)) {
	return -1;
    }
    return result;
}

#ifdef HAVE_ADDRESS_OF_LABEL

tsf_bool_t gpc_intable_run_supported() {
    return tsf_false;
}

uintptr_t gpc_intable_run(gpc_intable_t *prog,
                          void *region,
                          gpc_cell_t *args) {
    tsf_set_error(TSF_E_NOT_SUPPORTED,
                  "Non-threaded interpretation support was not compiled into "
                  "the TSF package.  This is because the threaded interpreter "
                  "was compiled instead.  Please use the threaded "
                  "interpreter.");
    return 0;
}

#else

tsf_bool_t gpc_intable_run_supported() {
    return tsf_true;
}

#include "gpc_int_common.h"

#define INT_BEGIN(inst_name) \
    case GPC_##inst_name: /* printf("%s\n",#inst_name); */ ++inst; {

#define INT_END() \
    } goto int_loop;

#define INT_ADVANCE(amount) \
    (inst+=(amount))

#define INT_GOTO(value) do {    \
    void *tmp=(void*)value;     \
    inst=tmp;                   \
    goto int_loop;              \
} while (0)

uintptr_t gpc_intable_run(gpc_intable_t *prog,
                          void *region,
                          gpc_cell_t *args) {
    gpc_cell_t *inst;
    uint8_t *buf=NULL,
            *cur=NULL,
            *end=NULL;
    uint32_t size=0;
    tsf_bool_t keep=tsf_false;
    tsf_type_in_map_t *types=NULL;
    
    tsf_type_out_map_t *out_map=NULL;
    
    tsf_bool_t keep_region=tsf_false;
    
    int8_t i8_tmp;
    uint8_t ui8_tmp;
    int16_t i16_tmp;
    uint16_t ui16_tmp;
    int32_t i32_tmp;
    uint32_t ui32_tmp;
    int64_t i64_tmp;
    uint64_t ui64_tmp;
    float f_tmp;
    float d_tmp;
    
    gpc_cell_t *stack;
    gpc_cell_t *stack_top;
    
    uint32_t i;

    stack=alloca(sizeof(gpc_cell_t)*prog->max_height);
    if (stack==NULL) {
        tsf_set_errno("Could not allocate stack");
        return 0;
    }
    
    stack_top=stack-1;
    
    for (i=0;i<prog->num_args;++i) {
        INT_PUSH();
        INT_STACK(0)=args[i];
    }
    
    INT_GOTO(prog->stream);

int_loop:
    switch (*inst) {
#include "gpc_interpreter.gen"
        default:
            tsf_abort("Bad instruction");
            break;
    }

bounds_error:
    tsf_set_error(TSF_E_PARSE_ERROR,"Bounds check failure");

failure:
    /* error! */
    if (keep) {
        free(buf);
    }
    if (keep_region) {
        tsf_region_free(region);
    }
    if (out_map!=NULL) {
        tsf_type_out_map_destroy(out_map);
    }
    return 0;
}

#endif



