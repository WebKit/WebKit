/*
 * Copyright (C) 2004, 2005 Filip Pizlo. All rights reserved.
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

tsf_ra_type_man_t *tsf_ra_type_man_create(void) {
    tsf_ra_type_man_t *result=malloc(sizeof(tsf_ra_type_man_t));
    result->out_map=tsf_st_init_typetable();
    if (result->out_map==NULL) {
	return NULL;
    }
    result->in_map=tsf_st_init_numtable();
    if (result->in_map==NULL) {
	tsf_st_free_table(result->out_map);
	return NULL;
    }
    result->type_codes=NULL;
    return result;
}

void tsf_ra_type_man_destroy(tsf_ra_type_man_t *man) {
    tsf_ra_tc_node_t *n;
    tsf_st_iterator iter;

    n=man->type_codes;
    while (n!=NULL) {
	tsf_ra_tc_node_t *victim=n;
	n=n->next;
	free(victim);
    }
    
    for (tsf_st_iterator_init(man->out_map,&iter);
	 tsf_st_iterator_valid(&iter);
	 tsf_st_iterator_next(&iter)) {
	tsf_type_t *type;
	tsf_ra_type_info_t *info;
	tsf_st_iterator_get(&iter,(char**)&type,(void*)&info);
	tsf_type_destroy(type);
	free(info);
    }

    tsf_st_free_table(man->in_map);
    tsf_st_free_table(man->out_map);
}

tsf_bool_t tsf_ra_type_man_insert(tsf_ra_type_man_t *man,
				  tsf_type_t *type,
				  uint32_t type_code,
				  uint64_t ref_count) {
    tsf_ra_type_info_t *info;
    
    info=malloc(sizeof(tsf_ra_type_info_t));
    if (info==NULL) {
        tsf_set_errno("Could not malloc tsf_ra_type_info_t");
        return tsf_false;
    }
    
    info->type_code=type_code;
    info->ref_count=ref_count;
    
    type=tsf_type_dup(type);
    
    if (tsf_st_add_direct(man->out_map,
                          (char*)type,
                          info)<0) {
        tsf_set_errno("Could not tsf_st_add_direct()");
        tsf_type_destroy(type);
        free(info);
        return tsf_false;
    }
    
    if (tsf_st_add_direct(man->in_map,
                          (void*)(uintptr_t)type_code,
                          type)) {
        tsf_type_t *to_delete;
        tsf_set_errno("Could not tsf_st_add_direct()");
        to_delete = type;
        tsf_st_delete(man->out_map,
                      (char**)&to_delete,
                      NULL);
        tsf_type_destroy(type);
        free(info);
        return tsf_false;
    }
    
    return tsf_true;
}

void tsf_ra_type_man_remove(tsf_ra_type_man_t *man,
			    tsf_type_t *type) {
    tsf_ra_type_info_t *info;
    uintptr_t type_code;
    tsf_ra_tc_node_t *n;
    tsf_type_t *type_to_delete;
    
    type_to_delete = type;
    tsf_assert(tsf_st_delete(man->out_map,
                             (char**)&type_to_delete,
                             (void**)&info));
    
    type_code = info->type_code;
    
    tsf_assert(tsf_st_delete(man->in_map,
                             (char**)&type_code,
                             NULL));
    
    n=malloc(sizeof(tsf_ra_tc_node_t));
    if (n!=NULL) {
        n->next=man->type_codes;
        n->type_code=type_code;
        man->type_codes=n;
    } /* else we would signal an error but that would be bad so we instead
       * just leak this type code. */
    
    tsf_type_destroy(type);
    free(info);
}

/* maybe this shouldn't be setting the error when the type isn't
   found?  afterall, all uses of this function currently set
   their own error code. */
tsf_type_t *tsf_ra_type_man_find(tsf_ra_type_man_t *man,
				 uint32_t type_code) {
    tsf_type_t *type;

    if (!tsf_st_lookup(man->in_map,
                       (char*)(uintptr_t)type_code,
                       (void**)&type)) {
	tsf_set_error(TSF_E_ELE_NOT_FOUND,
		      "Element not found in tsf_ra_type_man_find()");
        return NULL;
    }
    
    return type;
}

tsf_bool_t tsf_ra_type_man_has_type(tsf_ra_type_man_t *man,
				    tsf_type_t *type) {
    if (tsf_st_lookup(man->out_map,
                      (char*)type,
                      NULL)) {
        return tsf_true;
    }
    
    return tsf_false;
}

/* call this only once you know that you know about the type. */
uint32_t tsf_ra_type_man_get_type_code(tsf_ra_type_man_t *man,
				       tsf_type_t *type) {
    tsf_ra_type_info_t *info;
    
    tsf_assert(tsf_st_lookup(man->out_map,
                             (char*)type,
                             (void**)&info));
    
    return info->type_code;
}

tsf_bool_t tsf_ra_type_man_get_reference(tsf_ra_type_man_t *man,
					 tsf_type_t *type) {
    tsf_ra_type_info_t *info;
    
    if (!tsf_st_lookup(man->out_map,
                       (char*)type,
                       (void**)&info)) {
        uint32_t type_code;
        tsf_ra_tc_node_t *n=man->type_codes;
        
        if (n==NULL) {
            /* if there are none in the free list, then that means
             * that all contiguous type codes are used up to the
             * number of live type codes.  hence we can use the
             * num_entries field on either hash table as the next
             * type code. */
            type_code=man->out_map->num_entries;
        } else {
            type_code=n->type_code;
            man->type_codes=n->next;
        }
        
        if (!tsf_ra_type_man_insert(man,
				    type,
				    type_code,
				    1)) {
            man->type_codes=n;
            return tsf_false;
        }
        
        if (n!=NULL) {
            free(n);
        }
        
        return tsf_true;
    }
    
    info->ref_count++;
    
    return tsf_true;
}

void tsf_ra_type_man_remove_reference(tsf_ra_type_man_t *man,
				      tsf_type_t *type) {
    tsf_ra_type_info_t *info;
    
    tsf_assert(tsf_st_lookup(man->out_map,
                             (char*)type,
                             (void**)&info));
    
    if (--info->ref_count==0) {
        tsf_ra_type_man_remove(man,type);
    }
}

uint32_t tsf_ra_type_man_num_types(tsf_ra_type_man_t *man) {
    return man->out_map->num_entries;
}

tsf_bool_t tsf_ra_type_man_has_buf_types(tsf_ra_type_man_t *man,
					 tsf_buffer_t *buf) {
    uint32_t i;
    
    if (!tsf_ra_type_man_has_type(man,buf->type)) {
        return tsf_false;
    }
    
    for (i=0;
         i<tsf_type_in_map_get_num_types(buf->types);
         ++i) {
        if (!tsf_ra_type_man_has_type(man,
                 tsf_type_in_map_get_type(buf->types,i))) {
            return tsf_false;
        }
    }
    
    return tsf_true;
}

tsf_bool_t tsf_ra_type_man_get_buf_references(tsf_ra_type_man_t *man,
					      tsf_buffer_t *buf) {
    uint32_t i;

    if (!tsf_ra_type_man_get_reference(man,buf->type)) {
        return tsf_false;
    }
    
    for (i=0;
         i<tsf_type_in_map_get_num_types(buf->types);
         ++i) {
        if (!tsf_ra_type_man_get_reference(man,
                 tsf_type_in_map_get_type(buf->types,i))) {
            uint32_t j;
            for (j=0;j<i;++j) {
                tsf_ra_type_man_remove_reference(man,
                    tsf_type_in_map_get_type(buf->types,j));
            }
            tsf_ra_type_man_remove_reference(man,buf->type);
            return tsf_false;
        }
    }
    
    return tsf_true;
}

void tsf_ra_type_man_remove_buf_references(tsf_ra_type_man_t *man,
					   tsf_buffer_t *buf) {
    uint32_t i;
    
    tsf_ra_type_man_remove_reference(man,buf->type);
    
    for (i=0;
         i<tsf_type_in_map_get_num_types(buf->types);
         ++i) {
        tsf_ra_type_man_remove_reference(man,
            tsf_type_in_map_get_type(buf->types,i));
    }
}

tsf_ra_type_man_t *tsf_ra_type_man_read(tsf_reader_t reader,
					void *arg) {
    tsf_ra_type_man_t *result;
    uint32_t num_types;
    uint32_t type_code;
    uint32_t *type_codes;
    uint64_t ref_count;
    uint32_t n,i;
    tsf_bool_t tres;

    if (!reader(arg,&num_types,sizeof(num_types))) {
	return NULL;
    }

    num_types=ntohl(num_types);

    result=tsf_ra_type_man_create();
    if (result==NULL) {
	return NULL;
    }

    type_codes=malloc(sizeof(uint32_t)*num_types);
    if (type_codes==NULL) {
	tsf_set_errno("Could not map type code array");
	tsf_ra_type_man_destroy(result);
	return NULL;
    }

    for (n=num_types;n-->0;) {
	tsf_type_t *type;
	
	if (!reader(arg,&type_code,sizeof(type_code))) {
	    free(type_codes);
	    tsf_ra_type_man_destroy(result);
	    return NULL;
	}

        type_code=ntohl(type_code);
	type_codes[n]=type_code;

	if (!reader(arg,&ref_count,sizeof(ref_count))) {
	    free(type_codes);
	    tsf_ra_type_man_destroy(result);
	    return NULL;
	}

	ref_count=ntohll(ref_count);

	type=tsf_type_read(reader,arg,NULL);
	if (type==NULL) {
	    free(type_codes);
	    tsf_ra_type_man_destroy(result);
	    return NULL;
	}

	tres=tsf_ra_type_man_insert(result,
				    type,
				    type_code,
				    ref_count);

	tsf_type_destroy(type);

	if (!tres) {
	    free(type_codes);
	    tsf_ra_type_man_destroy(result);
	    return NULL;
	}
    }

    tsf_ui32_sort(type_codes,num_types);

    if (num_types>0) {
	for (i=0;i<num_types-1;++i) {
	    tsf_assert(type_codes[i]<type_codes[i+1]);
            
	    for (type_code=type_codes[i]+1;
		 type_code<type_codes[i+1];
		 ++type_code) {
		tsf_ra_tc_node_t *n=malloc(sizeof(tsf_ra_tc_node_t));
		if (n==NULL) {
		    free(type_codes);
		    tsf_ra_type_man_destroy(result);
		    return NULL;
		}
                
		n->next=result->type_codes;
		n->type_code=type_code;
		result->type_codes=n;
	    }
	}
    }

    free(type_codes);

    return result;
}

tsf_bool_t tsf_ra_type_man_write(tsf_ra_type_man_t *man,
				 tsf_writer_t writer,
				 void *arg) {
    uint32_t num_types;
    tsf_st_iterator iter;
    
    num_types=htonl(man->out_map->num_entries);
    if (!writer(arg,&num_types,sizeof(num_types))) {
	return tsf_false;
    }

    for (tsf_st_iterator_init(man->out_map,&iter);
	 tsf_st_iterator_valid(&iter);
	 tsf_st_iterator_next(&iter)) {
	tsf_type_t *type;
	tsf_ra_type_info_t *info;
	uint32_t type_code;
	uint64_t ref_count;
	tsf_st_iterator_get(&iter,(char**)&type,(void*)&info);
	type_code=htonl(info->type_code);
	ref_count=htonll(info->ref_count);
	if (!writer(arg,&type_code,sizeof(type_code))) {
	    return tsf_false;
	}
	if (!writer(arg,&ref_count,sizeof(ref_count))) {
	    return tsf_false;
	}
	if (!tsf_type_write(type,writer,arg)) {
	    return tsf_false;
	}
    }

    return tsf_true;
}

uint32_t tsf_ra_type_man_get_type_code_free_list_size(tsf_ra_type_man_t *man) {
    uint32_t ret=0;
    tsf_ra_tc_node_t *cur;
    
    for (cur=man->type_codes;
         cur!=NULL;
         cur=cur->next) {
        ++ret;
    }
    
    return ret;
}


