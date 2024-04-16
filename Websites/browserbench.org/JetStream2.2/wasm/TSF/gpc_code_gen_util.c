/*
 * Copyright (C) 2003, 2004, 2005 Filip Pizlo. All rights reserved.
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
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

/* code generation debugging */

/* NOTE: It is now the case that the count may be incremented multiple times,
 * in order to insure that files are not overwritten.  This allows multiple
 * processes to use the same directory for code gen debugging.  However, the
 * multi-increment appproach may be overkill, since I could have probably just
 * as well have used the PID. */

static volatile uint32_t count=0;

static tsf_once_t once_control = TSF_ONCE_INIT;
static tsf_mutex_t count_lock;

static void count_init(void) {
    tsf_mutex_init(&count_lock);
}

static uint32_t next_count() {
    uint32_t result;
    tsf_once(&once_control, count_init);
    tsf_mutex_lock(&count_lock);
    result=count++;
    tsf_mutex_unlock(&count_lock);
    return result;
}

void gpc_dyncc_code_gen_debug(const char *type,
                              const char *src_filename) {
    const char *tsf_code_gen_debug=getenv("TSF_CODE_GEN_DEBUG");
    const char *tsf_code_gen_debug_path;
    char *dest_filename;
    int fd_src,fd_trg;
    uint32_t cur_cnt;
    
    if (tsf_code_gen_debug==NULL || strcasecmp(tsf_code_gen_debug,"YES")) {
        return;
    }
    
    tsf_code_gen_debug_path=getenv("TSF_CODE_GEN_DEBUG_PATH");
    
    if (tsf_code_gen_debug_path==NULL) {
        return;
    }
    
    fd_src=open(src_filename,O_RDONLY);
    if (fd_src<0) {
        return;
    }
    
    /* loop until we hit a filename that isn't in use */
    for (cur_cnt=count;;cur_cnt=next_count()) {
        dest_filename=tsf_asprintf("%s/" fui32 "_%s",
                                   tsf_code_gen_debug_path,
                                   cur_cnt,
                                   type);
        if (dest_filename==NULL) {
            close(fd_src);
            return;
        }
        
        fd_trg=open(dest_filename,O_CREAT|O_EXCL|O_WRONLY,0600);
        free(dest_filename);
        
        if (fd_trg>=0) {
            break;
        }
        
        if (errno!=EEXIST) {
            close(fd_src);
            return;
        }
    }
    
    for (;;) {
        char buf[1024];
        int res=read(fd_src,buf,sizeof(buf));
        if (res<=0) {
            break;
        }
        if (write(fd_trg,buf,res)!=res) {
            break;
        }
    }
    
    close(fd_src);
    close(fd_trg);
}

void gpc_code_gen_debug(const char *type,
                        gpc_proto_t *proto) {
    const char *tsf_code_gen_debug=getenv("TSF_CODE_GEN_DEBUG");
    const char *tsf_code_gen_debug_path;
    char *dest_filename;
    FILE *out;
    uint32_t cur_cnt;
    
    /* printf("dumping %s\n",type); */

    if (tsf_code_gen_debug==NULL || strcasecmp(tsf_code_gen_debug,"YES")) {
        return;
    }
    
    tsf_code_gen_debug_path=getenv("TSF_CODE_GEN_DEBUG_PATH");
    
    if (tsf_code_gen_debug_path==NULL) {
        return;
    }
    
    for (cur_cnt=count;;cur_cnt=next_count()) {
        int fd;
        
        dest_filename=tsf_asprintf("%s/" fui32 "_%s",
                                   tsf_code_gen_debug_path,
                                   cur_cnt,
                                   type);
        if (dest_filename==NULL) {
            return;
        }
        
        fd=open(dest_filename,O_CREAT|O_EXCL|O_WRONLY,0600);
        free(dest_filename);

        if (fd>=0) {
            out=fdopen(fd,"w");
            
            if (out==NULL) {
		close(fd);
                return;
            }
            
            break;
        }
        
        if (errno!=EEXIST) {
            /* printf("error not eexist\n"); */
            return;
        }
    }
    
    gpc_proto_print(proto,out);
    
    fclose(out);
}

/* stupid stuff */

void gpc_unlink_free(char *name) {
    unlink(name);
    free(name);
}

void gpc_rmdir_free(char *name) {
    rmdir(name);
    free(name);
}

/* this isn't a real rm -rf; it only goes down one level. */
void gpc_rmdashrf_free(char *name) {
    DIR *dir;
    dir=opendir(name);
    if (dir!=NULL) {
	for (;;) {
	    struct dirent entry,*result=NULL;
	    int res=readdir_r(dir,&entry,&result);
	    if (res!=0 || result==NULL) {
		break;
	    }
	    if (strcmp(entry.d_name,".") &&
		strcmp(entry.d_name,"..")) {
		char *flnm=tsf_asprintf("%s/%s",name,entry.d_name);
		if (flnm!=NULL) {
		    unlink(flnm);
		    free(flnm);
		}
	    }
	}
	closedir(dir);
	rmdir(name);
    }
    free(name);
}

