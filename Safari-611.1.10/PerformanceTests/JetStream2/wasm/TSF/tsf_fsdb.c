/*
 * Copyright (C) 2011, 2014, 2015 Filip Pizlo. All rights reserved.
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
#include "tsf_format.h"
#include "tsf_fsdb_protocol.h"

#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_BSDSOCKS
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#endif

#ifdef HAVE_BSDSOCKS
static tsf_fsdb_response_t *connection_read(tsf_fsdb_connection_t *con);

static tsf_bool_t open_connection(int domain,
                                  int type,
                                  int protocol,
                                  const struct sockaddr *addr,
                                  socklen_t addrlen,
                                  int *fd) {
    int flag;
    
    *fd=socket(domain,type,protocol);
    if (*fd<0) {
        tsf_set_errno("Could not create socket with domain %d, type %d, protocol %d",
                      domain,type,protocol);
        return tsf_false;
    }
    
    if (connect(*fd,addr,addrlen)<0) {
        tsf_set_errno("Could not establish connection");
        close(*fd);
        return tsf_false;
    }
    
    flag=1;
    if (setsockopt(*fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag))<0) {
        tsf_set_errno("Could not disable Nagle's algorithm");
        close(*fd);
        return tsf_false;
    }
    
    return tsf_true;
}

static tsf_fsdb_connection_t *connection_create(int fd,
                                                tsf_fsdb_t *fsdb) {
    tsf_fsdb_connection_t *result;
    tsf_fsdb_response_t *rsp;
    
    tsf_assert(fd>=0);
    
    result=malloc(sizeof(tsf_fsdb_connection_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_fsdb_connection_t");
        goto error1;
    }
    
    result->buf_size=0;
    result->buf_cursor=0;
    result->buf=NULL;
    
    result->fd=fd;
    
    result->in=tsf_stream_file_input_fd_open(
        fd,fsdb->limits,&fsdb->rdr_attr,tsf_false,TSF_DEF_NET_BUF_SIZE);
    if (result->in==NULL) {
        goto error2;
    }
    
    result->out=tsf_stream_file_output_fd_open(
        fd,&fsdb->wtr_attr,tsf_false,TSF_DEF_NET_BUF_SIZE);
    if (result->out==NULL) {
        goto error3;
    }
    
    rsp=connection_read(result);
    if (rsp==NULL) {
        goto error4;
    }
    
    if (rsp->payload.value!=tsf_fsdb_response__payload__welcome) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Bad response from FSDB server: response %s with state %s",
                      tsf_fsdb_response__payload__to_str(rsp->payload.value),
                      tsf_fsdb_state__to_str(rsp->state.value));
        tsf_region_free(rsp);
        goto error4;
    }
    
    tsf_region_free(rsp);
    
    return result;

error4:
    tsf_stream_file_output_close(result->out);
error3:
    tsf_stream_file_input_close(result->in);
error2:
    free(result);
error1:
    close(fd);
    return NULL;
}

static void connection_destroy(tsf_fsdb_connection_t *con) {
    if (con->buf!=NULL) {
        free(con->buf);
    }
    tsf_stream_file_input_close(con->in);
    tsf_stream_file_output_close(con->out);
    close(con->fd);
    free(con);
}

static tsf_fsdb_response_t *connection_read(tsf_fsdb_connection_t *con) {
    tsf_fsdb_response_t *rsp=tsf_fsdb_response__read(con->in);
    if (rsp==NULL) {
        return NULL;
    }
    
    con->clear=(rsp->state.value==tsf_fsdb_state__clear);
    
    return rsp;
}

static void connection_return(tsf_fsdb_t *fsdb,
                              tsf_fsdb_connection_t *con) {
    if (con->clear) {
        tsf_mutex_lock(&fsdb->lock);
        if (fsdb->u.remote.connection==NULL) {
            fsdb->u.remote.connection=con;
            con=NULL;
        }
        tsf_mutex_unlock(&fsdb->lock);
    }
    
    if (con!=NULL) {
        connection_destroy(con);
    }
}

static tsf_fsdb_connection_t *create_first_connection(tsf_fsdb_t *fsdb,
                                                      const char *hostname,
                                                      int port) {
    int fd=-1;
    tsf_bool_t connected=tsf_false;
    
#ifdef HAVE_GETADDRINFO
    struct addrinfo hints;
    struct addrinfo *result,*cur;
    char buf[32];
    
    bzero(&hints,sizeof(hints));
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    
    snprintf(buf,sizeof(buf),"%d",port);
    
    if (getaddrinfo(hostname,buf,&hints,&result)!=0) {
        tsf_set_error(TSF_E_HOSTNAME,
                      "Could not lookup hostname %s",
                      hostname);
        return NULL;
    }
    
    for (cur=result;cur!=NULL;cur=cur->ai_next) {
        if (open_connection(cur->ai_family,
                            cur->ai_socktype,
                            cur->ai_protocol,
                            cur->ai_addr,
                            cur->ai_addrlen,
                            &fd)) {
            connected=tsf_true;
            fsdb->u.remote.result=result;
            fsdb->u.remote.cur=cur;
            break;
        }
    }
    
    if (!connected) {
        freeaddrinfo(result);
    }
#else
    struct hostent *hp = gethostbyname(hostname);

    if (hp == NULL) {
        tsf_set_error(TSF_E_HOSTNAME,
                      "Could not lookup hostname %s",
                      hostname);
        return NULL;
    } else {
        unsigned i;
        for (i=0;hp->h_addr_list[i]!=NULL;++i) {
            struct sockaddr_in addr;
            socklen_t slen=sizeof(addr);
            
            bzero(&addr,slen);
            addr.sin_family=AF_INET;
            addr.sin_port=htons(port);
            addr.sin_addr.s_addr=*(struct in_addr*)(hp->h_addr_list[i]);
            
            if (open_connection(AF_INET,
                                SOCK_STREAM,
                                0,
                                &addr,
                                slen,
                                &fd)) {
                connected=tsf_true;
                fsdb->u.remote.addr=*(uint32_t*)(hp->h_addr_list[i]);
                fsdb->u.remote.port=port;
                break;
            }
        }
    }
#endif
    
    if (!connected) {
        return NULL;
    }
    
    return connection_create(fd,fsdb);
}

static tsf_fsdb_connection_t *create_connection(tsf_fsdb_t *fsdb) {
    int fd;

#ifdef HAVE_GETADDRINFO
    if (!open_connection(fsdb->u.remote.cur->ai_family,
                         fsdb->u.remote.cur->ai_socktype,
                         fsdb->u.remote.cur->ai_protocol,
                         fsdb->u.remote.cur->ai_addr,
                         fsdb->u.remote.cur->ai_addrlen,
                         &fd)) {
        return NULL;
    }
#else
    struct sockaddr_in addr;
    socklen_t slen=sizeof(addr);
    
    bzero(&addr,slen);
    addr.sin_family=AF_INET;
    addr.sin_port=htons(fsdb->u.remote.port);
    addr.sin_addr.s_addr=*(struct in_addr*)(&fsdb->u.remote.addr);
            
    if (!open_connection(AF_INET,
                         SOCK_STREAM,
                         0,
                         &addr,
                         slen,
                         &fd)) {
        return NULL;
    }
#endif
    
    return connection_create(fd,fsdb);
}

static tsf_fsdb_response_t *first_action(tsf_fsdb_t *fsdb,
                                         tsf_fsdb_connection_t **connection,
                                         tsf_fsdb_command_t *cmd) {
    unsigned i;
    tsf_mutex_lock(&fsdb->lock);
    *connection=fsdb->u.remote.connection;
    fsdb->u.remote.connection=NULL;
    tsf_mutex_unlock(&fsdb->lock);
    
    for (i=0;;++i) {
        if (*connection!=NULL &&
            tsf_fsdb_command__write((*connection)->out,cmd) &&
            tsf_stream_file_output_flush((*connection)->out)) {
            tsf_fsdb_response_t *response=connection_read(*connection);
            if (response!=NULL) {
                return response;
            }
        }
        
        if (i==0) {
            /* retry - the connection may have gone stale */
            if (*connection!=NULL) {
                connection_destroy(*connection);
            }
            
            *connection=create_connection(fsdb);
            if (*connection==NULL) {
                return NULL;
            }
        } else {
            /* already tried before, now fail permanently and destroy the connection. */
            tsf_assert(*connection!=NULL);
            connection_destroy(*connection);
            *connection=NULL;
            return NULL;
        }
    }
}

static void set_error(tsf_fsdb_response_t *rsp) {
    switch (rsp->payload.value) {
    case tsf_fsdb_response__payload__sys_error:
        tsf_set_error(TSF_E_EXTERNAL,
                      "FSDB server responded with system error: %s",
                      rsp->payload.u.sys_error.msg);
        break;
    case tsf_fsdb_response__payload__proto_error:
        tsf_set_error(TSF_E_EXTERNAL,
                      "FSDB server responded with protocol error: %s",
                      rsp->payload.u.proto_error.msg);
        break;
    case tsf_fsdb_response__payload__not_found:
        tsf_set_error(TSF_E_ELE_NOT_FOUND,
                      "FSDB server could not find the requested element");
        break;
    case tsf_fsdb_response__payload__in_eof:
        tsf_set_error(TSF_E_EOF,
                      "End of file from FSDB server");
        break;
    default:
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Unexpected or unrecognized response from FSDB server; "
                      "state = %s, response = %s",
                      tsf_fsdb_state__to_str(rsp->state.value),
                      tsf_fsdb_response__payload__to_str(rsp->payload.value));
        break;
    }
}

static tsf_bool_t network_writer_flush(tsf_fsdb_out_t *out) {
    tsf_fsdb_command_t cmd;
    tsf_fsdb_response_t *rsp;
    
    cmd.value=tsf_fsdb_command__out_write;
    cmd.u.out_write.data.data=out->u.remote.connection->buf;
    cmd.u.out_write.data.len=out->u.remote.connection->buf_cursor;
    if (!tsf_fsdb_command__write(out->u.remote.connection->out,
                                 &cmd) ||
        !tsf_stream_file_output_flush(out->u.remote.connection->out)) {
        return tsf_false;
    }
    
    rsp=connection_read(out->u.remote.connection);
    if (rsp==NULL) {
        return tsf_false;
    }
    
    if (rsp->state.value!=tsf_fsdb_state__put ||
        rsp->payload.value!=tsf_fsdb_response__payload__ok) {
        set_error(rsp);
        tsf_region_free(rsp);
        return tsf_false;
    }
    
    tsf_region_free(rsp);
    
    out->u.remote.connection->buf_cursor=0;
    
    return tsf_true;
}

static tsf_bool_t network_writer(void *arg,
                                 const void *buf_,
                                 uint32_t len) {
    tsf_fsdb_out_t *out=(tsf_fsdb_out_t*)arg;
    uint8_t *buf = (uint8_t*)buf_;
    uint32_t towrite;
    
    while (len>0) {
        towrite=tsf_min(len,
                        out->u.remote.connection->buf_size-
                        out->u.remote.connection->buf_cursor);
        memcpy(out->u.remote.connection->buf+
               out->u.remote.connection->buf_cursor,
               buf,
               towrite);
        
        out->u.remote.connection->buf_cursor+=towrite;
        buf+=towrite;
        len-=towrite;
        
        if (len==0) {
            break;
        }
        
        if (!network_writer_flush(out)) {
            return tsf_false;
        }
    }
    
    return tsf_true;
}

static tsf_bool_t network_reader(void *arg,
                                 void *buf_,
                                 uint32_t len) {
    tsf_fsdb_in_t *in=(tsf_fsdb_in_t*)arg;
    uint8_t *buf=(uint8_t*)buf_;
    
    while (len>0) {
        if (in->u.remote.rsp!=NULL) {
            uint32_t toread=
                tsf_min(len,
                        in->u.remote.rsp->payload.u.in_data.data.len-
                        in->u.remote.rsp_cursor);
            
            memcpy(buf,
                   in->u.remote.rsp->payload.u.in_data.data.data+in->u.remote.rsp_cursor,
                   toread);
            
            in->u.remote.rsp_cursor+=toread;
            len-=toread;
            buf+=toread;

            if (len==0) {
                break;
            }
        }
        
        if (in->u.remote.rsp!=NULL &&
            in->u.remote.rsp_cursor==in->u.remote.rsp->payload.u.in_data.data.len) {
            
            tsf_region_free(in->u.remote.rsp);
            in->u.remote.rsp=NULL;
            in->u.remote.rsp_cursor=0;
        }

        if (in->u.remote.rsp==NULL) {
            in->u.remote.rsp_cursor=0;
            in->u.remote.rsp=
                connection_read(in->u.remote.connection);
            
            switch (in->u.remote.rsp->payload.value) {
            case tsf_fsdb_response__payload__in_data:
                /* ok! */
                break;
            case tsf_fsdb_response__payload__in_eof:
                tsf_set_error((void*)buf==buf_?
                              TSF_E_UNEXPECTED_EOF:TSF_E_ERRONEOUS_EOF,
                              "EOF in response from FSDB server");
                tsf_region_free(in->u.remote.rsp);
                in->u.remote.rsp=NULL;
                return tsf_false;
            default:
                set_error(in->u.remote.rsp);
                tsf_region_free(in->u.remote.rsp);
                in->u.remote.rsp=NULL;
                return tsf_false;
            }
        }
    }
    
    return tsf_true;
}
#endif

static void copy_params(tsf_fsdb_t *fsdb,
                        tsf_limits_t *limits,
                        tsf_zip_rdr_attr_t *rdr_attr,
                        tsf_zip_wtr_attr_t *wtr_attr) {
    fsdb->limits=limits;
    
    if (rdr_attr==NULL) {
        tsf_zip_rdr_attr_get_default(&fsdb->rdr_attr);
    } else {
        fsdb->rdr_attr=*rdr_attr;
    }
    
    if (wtr_attr==NULL) {
        tsf_zip_wtr_attr_get_default(&fsdb->wtr_attr);
    } else {
        fsdb->wtr_attr=*wtr_attr;
    }
}

tsf_fsdb_t *tsf_fsdb_open_local(const char *dirname,
                                tsf_limits_t *limits,
                                tsf_zip_rdr_attr_t *rdr_attr,
                                tsf_zip_wtr_attr_t *wtr_attr,
                                tsf_bool_t update,
                                tsf_bool_t truncate) {
    tsf_fsdb_t *result;
    char *marker_flnm;
    FILE *fin;
    char buf[9];
    
    if (!update && truncate) {
        tsf_set_error(TSF_E_INVALID_ARG,
                      "Cannot specify the truncate without the update "
                      "flag");
        return NULL;
    }
    
    result=malloc(sizeof(tsf_fsdb_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_fsdb_t");
        return NULL;
    }
    
    result->kind=TSF_FSDB_LOCAL;
    
    result->u.local.dirname=strdup(dirname);
    if (result->u.local.dirname==NULL) {
        tsf_set_errno("Could not strdup dirname in tsf_fsdb_open_local");
        goto error1;
    }
    
    if (update) {
        if (mkdir(result->u.local.dirname,0777)<0) {
            if (errno==EEXIST) {
                /* all good. */
            } else {
                tsf_set_errno("Could not create fsdb directory %s",result->u.local.dirname);
                goto error2;
            }
        } else {
            /* directory created; now create the marker file. */
            FILE *fout;
            
            marker_flnm=tsf_asprintf("%s/TSF_FSDB",result->u.local.dirname);
            if (marker_flnm==NULL) {
                tsf_set_errno("Could not allocate string");
                goto error2;
            }
            
            fout=fopen(marker_flnm,"w");
            if (fout==NULL) {
                tsf_set_errno("Could not open %s",marker_flnm);
                free(marker_flnm);
                goto error2;
            }
            
            free(marker_flnm);
            
            fputs("TSF_FSDB",fout); /* FIXME: error checking? */
            
            fclose(fout);
        }
    }
    
    marker_flnm=tsf_asprintf("%s/TSF_FSDB",result->u.local.dirname);
    if (marker_flnm==NULL) {
        tsf_set_errno("Could not allocate string");
        goto error2;
    }
    
    fin=fopen(marker_flnm,"r");
    if (fin==NULL) {
        tsf_set_errno("Could not open %s",marker_flnm);
        free(marker_flnm);
        goto error2;
    }
    
    free(marker_flnm);
    
    bzero(buf,sizeof(buf));
    fgets(buf,sizeof(buf),fin);
    
    fclose(fin);
    
    if (strcmp(buf,"TSF_FSDB")) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Bad FSDB database: the marker file contains %s "
                      "instead of TSF_FSDB",
                      buf);
        goto error2;
    }

    /* make sure that it exists and is a directory, and keep a handle to it
       just for good measure. */
    
    result->u.local.dirhandle=opendir(result->u.local.dirname);
    if (result->u.local.dirhandle==NULL) {
        tsf_set_errno("Could not open fsdb directory %s",result->u.local.dirname);
        goto error2;
    }
    
    /* if we're truncating, then delete all of the files. */
    if (truncate) {
        struct dirent *entry;

#if defined(HAVE_READDIR_R) && defined(HAVE_PATHCONF)
        struct dirent *entrybuf;
        long limit;
        size_t len;
        
        limit=pathconf(result->u.local.dirname,_PC_PATH_MAX);
        if (limit<0) {
            tsf_set_errno("Could not get pathconf _PC_PATH_MAX for %s",
                          result->u.local.dirname);
            goto error3;
        }
        
        len=tsf_offsetof(struct dirent,d_name)+limit+1;
        
        entrybuf=malloc(len);
        if (entrybuf==NULL) {
            tsf_set_errno("Could not malloc struct dirent with size " fsz,
                          len);
            goto error3;
        }
#endif
        
        for (;;) {
            char *flnm;
            
#if defined(HAVE_READDIR_R) && defined(HAVE_PATHCONF)
            entry=NULL;
            if (readdir_r(result->u.local.dirhandle,entrybuf,&entry)!=0) {
                tsf_set_errno("Could not readdir_r in %s",
                              result->u.local.dirname);
                free(entrybuf);
                goto error3;
            }
#else
            errno=0;
            entry=readdir(result->u.local.dirhandle);
            if (entry==NULL && errno!=0) {
                tsf_set_errno("Could not readdir in %s",
                              result->u.local.dirname);
                goto error3;
            }
#endif
            
            if (entry==NULL) {
                break;
            }
            
            if (strcmp(entry->d_name,".") &&
                strcmp(entry->d_name,"..") &&
                strcmp(entry->d_name,"TSF_FSDB")) {

                flnm=tsf_asprintf("%s/%s",result->u.local.dirname,entry->d_name);
                if (flnm==NULL) {
                    tsf_set_errno("Could not tsf_asprintf");
#if defined(HAVE_READDIR_R) && defined(HAVE_PATHCONF)
                    free(entrybuf);
#endif
                    goto error3;
                }
                
                if (unlink(flnm)<0) {
                    tsf_set_errno("Could not unlink %s",flnm);
#if defined(HAVE_READDIR_R) && defined(HAVE_PATHCONF)
                    free(entrybuf);
#endif
                    free(flnm);
                    goto error3;
                }
                
                free(flnm);
            }
        }
        
#if defined(HAVE_READDIR_R) && defined(HAVE_PATHCONF)
        free(entrybuf);
#endif
    }

    copy_params(result,limits,rdr_attr,wtr_attr);
    
    result->u.local.update=update;
    result->u.local.truncate=truncate;
    
    result->u.local.create_cnt=(((int64_t)time(NULL))<<32)+(((int64_t)getpid())<<48);
    
    tsf_mutex_init(&result->lock);
    
    return result;

error3:
    closedir(result->u.local.dirhandle);
error2:
    free((void*)result->u.local.dirname);
error1:
    free(result);
    return NULL;
}

tsf_fsdb_t *tsf_fsdb_open_remote(const char *host,
                                 int port,
                                 tsf_limits_t *limits,
                                 tsf_zip_rdr_attr_t *rdr_attr,
                                 tsf_zip_wtr_attr_t *wtr_attr) {
#ifdef HAVE_BSDSOCKS
    tsf_fsdb_t *result;
    
    result=malloc(sizeof(tsf_fsdb_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_fsdb_t");
        return NULL;
    }
    
    result->kind=TSF_FSDB_REMOTE;
    
    copy_params(result,limits,rdr_attr,wtr_attr);
    
    result->u.remote.connection=create_first_connection(result,host,port);
    if (result->u.remote.connection==NULL) {
        free(result);
        return NULL;
    }
    
    tsf_mutex_init(&result->lock);
    
    return result;
#else
    tsf_set_error(TSF_E_NOT_SUPPORTED,
                  "BSD sockets support was not found on your system");
    return NULL;
#endif
}

void tsf_fsdb_close(tsf_fsdb_t *fsdb) {
    tsf_mutex_destroy(&fsdb->lock);
    
    switch (fsdb->kind) {
    case TSF_FSDB_LOCAL:
        closedir(fsdb->u.local.dirhandle);
        free((void*)fsdb->u.local.dirname);
        break;
#ifdef HAVE_BSDSOCKS
    case TSF_FSDB_REMOTE:
#ifdef HAVE_GETADDRINFO
        freeaddrinfo(fsdb->u.remote.result);
#endif
        if (fsdb->u.remote.connection!=NULL) {
            connection_destroy(fsdb->u.remote.connection);
        }
        break;
#endif
    default:
        tsf_abort("bad FSDB kind");
    }
    free(fsdb);
}

tsf_bool_t tsf_fsdb_guts_put(tsf_fsdb_t *fsdb,
                             tsf_buffer_t *key,
                             int *fd,
                             char **partflnm) {
    tsf_stream_file_output_t *out;
    
    if (fsdb==NULL || key==NULL) {
        return tsf_false;
    }
    
    tsf_assert(fsdb->kind==TSF_FSDB_LOCAL);
    
    if (!fsdb->u.local.update) {
        tsf_set_error(TSF_E_NOT_ALLOWED,
                      "Cannot put into FSDB because update flag was not set.");
        return tsf_false;
    }
    
    for (;;) {
        uint64_t my_id;
        
        tsf_mutex_lock(&fsdb->lock);
        my_id=fsdb->u.local.create_cnt;
        fsdb->u.local.create_cnt+=(uint64_t)(uintptr_t)fd;
        tsf_mutex_unlock(&fsdb->lock);
        
        *partflnm=tsf_asprintf("%s/part-" fui64,fsdb->u.local.dirname,my_id);
        if (*partflnm==NULL) {
            tsf_set_errno("Could not tsf_asprintf in tsf_fsdb_put");
            return tsf_false;
        }
        
        *fd=open(*partflnm,O_CREAT|O_EXCL|O_WRONLY|O_TRUNC,0666);
        if (*fd<0) {
            if (errno==EEXIST) {
                free(*partflnm);
                continue;
            }
            tsf_set_errno("Could not open %s",*partflnm);
            free(*partflnm);
            return tsf_false;
        }
        
        break;
    }
    
    /* ok, now write the key as the first record of the file. */
    out=tsf_stream_file_output_fd_open(*fd,NULL,tsf_false,0);
    if (out==NULL) {
        goto abort;
    }
    
    if (!tsf_stream_file_output_write(out,key)) {
        tsf_stream_file_output_close(out);
        goto abort;
    }
    
    if (!tsf_stream_file_output_close(out)) {
        goto abort;
    }
    
    return tsf_true;
abort:
    tsf_fsdb_guts_abort(fsdb,*partflnm);
    free(*partflnm);
    close(*fd);
    return tsf_false;
}

tsf_bool_t tsf_fsdb_guts_commit(tsf_fsdb_t *fsdb,
                                char *partflnm,
                                tsf_buffer_t *key,
                                char **dataflnm) {
    char *name;
    char *flnm;
    uint32_t sha1sum[5];
    
    if (fsdb==NULL || partflnm==NULL || key==NULL) {
        return tsf_false;
    }
    
    tsf_assert(fsdb->kind==TSF_FSDB_LOCAL);
    
    if (!fsdb->u.local.update) {
        tsf_set_error(TSF_E_NOT_ALLOWED,
                      "Cannot commit into FSDB because update flag was not set.");
        return tsf_false;
    }
    
    if (!tsf_buffer_calc_sha1(key,sha1sum)) {
        return tsf_false;
    }
    
    name=tsf_sha1_sum_to_str(sha1sum);
    if (name==NULL) {
        return tsf_false;
    }
    
    flnm=tsf_asprintf("%s/data-%s",fsdb->u.local.dirname,name);
    if (flnm==NULL) {
        tsf_set_errno("Could not asprintf in tsf_fsdb_guts_commit");
        free(name);
        return tsf_false;
    }
    
    free(name);
    
    if (rename(partflnm,flnm)<0) {
        tsf_set_errno("Could not rename %s to %s",
                      partflnm,flnm);
        free(flnm);
        return tsf_false;
    }
    
    if (dataflnm!=NULL) {
        *dataflnm=flnm;
    } else {
        free(flnm);
    }
    
    return tsf_true;
}

tsf_bool_t tsf_fsdb_guts_abort(tsf_fsdb_t *fsdb,
                               char *partflnm) {
    if (fsdb==NULL || partflnm==NULL) {
        return tsf_false;
    }
    
    tsf_assert(fsdb->kind==TSF_FSDB_LOCAL);
    
    if (!fsdb->u.local.update) {
        tsf_set_error(TSF_E_NOT_ALLOWED,
                      "Cannot abort FSDB file because update flag was not set.");
        return tsf_false;
    }
    
    unlink(partflnm);
    
    return tsf_true;
}

tsf_bool_t tsf_fsdb_guts_get(tsf_fsdb_t *fsdb,
                             tsf_buffer_t *key,
                             int *fd,
                             char **dataflnm) {
    uint32_t sha1sum[5];
    char *name;
    char *flnm;
    tsf_serial_in_man_t *in_man;
    tsf_buf_rdr_t *reader;
    tsf_buffer_t *read_key;
    uint32_t num_remaining;

    if (fsdb==NULL || key==NULL) {
        /* propagate error */
        return tsf_false;
    }
    
    tsf_assert(fsdb->kind==TSF_FSDB_LOCAL);
    
    if (!tsf_buffer_calc_sha1(key,sha1sum)) {
        return tsf_false;
    }
    
    name=tsf_sha1_sum_to_str(sha1sum);
    if (name==NULL) {
        return tsf_false;
    }
    
    flnm=tsf_asprintf("%s/data-%s",fsdb->u.local.dirname,name);
    if (flnm==NULL) {
        tsf_set_errno("Could not asprintf in tsf_fsdb_get");
        free(name);
        return tsf_false;
    }
    
    free(name);
    
    *fd=open(flnm,O_RDONLY);
    if (*fd<0) {
        if (errno==ENOENT) {
            tsf_set_error(TSF_E_ELE_NOT_FOUND,
                          "The requested key was not found in the FSDB database");
        } else {
            tsf_set_errno("Could not open %s for reading",flnm);
        }
        free(flnm);
        return tsf_false;
    }
    
    if (dataflnm!=NULL) {
        *dataflnm=flnm;
    } else {
        free(flnm);
    }
    
    /* now verify that the key is what it is supposed to be.  use a lower-level
       approach to reading so we can control how much buffering is done. */
    reader=tsf_buf_reader_create(*fd,128,tsf_false);
    if (reader==NULL) {
        goto fail;
    }
    
    in_man=tsf_serial_in_man_create(tsf_buf_reader_read,reader,fsdb->limits);
    if (in_man==NULL) {
        tsf_buf_reader_destroy(reader);
        goto fail;
    }
    
    read_key=tsf_serial_in_man_read(in_man);
    if (read_key==NULL) {
        tsf_serial_in_man_destroy(in_man);
        tsf_buf_reader_destroy(reader);
        goto fail;
    }
    
    tsf_serial_in_man_destroy(in_man);
    
    num_remaining=tsf_buf_reader_get_remaining(reader);
    tsf_buf_reader_destroy(reader);
    
    if (lseek(*fd,-((off_t)num_remaining),SEEK_CUR)<0) {
        tsf_set_errno("Could not unseek buffered data");
        tsf_buffer_destroy(read_key);
        goto fail;
    }
    
    if (!tsf_buffer_compare(key,read_key)) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Key mismatch on entry in FSDB database");
        tsf_buffer_destroy(read_key);
        goto fail;
    }
    
    tsf_buffer_destroy(read_key);
    
    return tsf_true;

fail:
    if (dataflnm!=NULL) {
        free(*dataflnm);
    }
    close(*fd);
    return tsf_false;
}

tsf_bool_t tsf_fsdb_guts_rm(tsf_fsdb_t *fsdb,
                            tsf_buffer_t *key,
                            char **dataflnm) {
    uint32_t sha1sum[5];
    char *name;
    char *flnm;

    if (fsdb==NULL || key==NULL) {
        /* propagate error */
        return tsf_false;
    }
    
    tsf_assert(fsdb->kind==TSF_FSDB_LOCAL);
    
    if (!fsdb->u.local.update) {
        tsf_set_error(TSF_E_NOT_ALLOWED,
                      "Cannot rm from FSDB because update flag was not set.");
        return tsf_false;
    }
    
    if (!tsf_buffer_calc_sha1(key,sha1sum)) {
        return tsf_false;
    }
    
    name=tsf_sha1_sum_to_str(sha1sum);
    if (name==NULL) {
        return tsf_false;
    }
    
    flnm=tsf_asprintf("%s/data-%s",fsdb->u.local.dirname,name);
    if (flnm==NULL) {
        tsf_set_errno("Could not asprintf in tsf_fsdb_get");
        free(name);
        return tsf_false;
    }
    
    free(name);
    
    if (unlink(flnm)<0) {
        if (errno==ENOENT) {
            tsf_set_error(TSF_E_ELE_NOT_FOUND,
                          "The requested key was not found in the FSDB database");
        } else {
            tsf_set_errno("Could not unlink %s",flnm);
        }
        free(flnm);
        return tsf_false;
    }

    if (dataflnm!=NULL) {
        *dataflnm=flnm;
    } else {
        free(flnm);
    }
    return tsf_true;
}

tsf_fsdb_out_t *tsf_fsdb_put(tsf_fsdb_t *fsdb,
                             tsf_buffer_t *key) {
    if (fsdb==NULL || key==NULL) {
        return NULL;
    }
    
    switch (fsdb->kind) {
    case TSF_FSDB_LOCAL: {
        int fd;
        char *flnm;
        tsf_fsdb_out_t *result;
    
        if (!tsf_fsdb_guts_put(fsdb,key,&fd,&flnm)) {
            return NULL;
        }
    
        result=malloc(sizeof(tsf_fsdb_out_t));
        if (result==NULL) {
            tsf_set_errno("Could not malloc in tsf_fsdb_put");
            close(fd);
            tsf_fsdb_guts_abort(fsdb,flnm);
            free(flnm);
            return NULL;
        }
    
        result->db=fsdb;
        result->u.local.partname=flnm;
    
        result->u.local.out=tsf_stream_file_output_fd_open(fd,
                                                           &fsdb->wtr_attr,
                                                           tsf_true,
                                                           0);
        if (result->u.local.out==NULL) {
            close(fd);
            tsf_fsdb_guts_abort(fsdb,flnm);
            free(flnm);
            free(result);
            return NULL;
        }
    
        result->u.local.key=tsf_buffer_dup(key);
        if (result->u.local.key==NULL) {
            tsf_stream_file_output_close(result->u.local.out);
            tsf_fsdb_guts_abort(fsdb,flnm);
            free(flnm);
            free(result);
            return NULL;
        }
    
        return result;
    }
#ifdef HAVE_BSDSOCKS
    case TSF_FSDB_REMOTE: {
        tsf_fsdb_connection_t *connection;
        tsf_fsdb_command_t cmd;
        tsf_fsdb_response_t *rsp;
        tsf_fsdb_out_t *result;
        
        cmd.value=tsf_fsdb_command__put;
        tsf_assert(key!=NULL);
        cmd.u.put.key=key;
        
        rsp=first_action(fsdb,&connection,&cmd);
        if (rsp==NULL) {
            return NULL;
        }
        
        if (rsp->state.value!=tsf_fsdb_state__put ||
            rsp->payload.value!=tsf_fsdb_response__payload__ok) {
            set_error(rsp);
            connection_destroy(connection);
            tsf_region_free(rsp);
            return NULL;
        }
        
        tsf_region_free(rsp);
        
        result=malloc(sizeof(tsf_fsdb_out_t));
        if (result==NULL) {
            tsf_set_errno("Could not malloc in tsf_fsdb_put");
            connection_destroy(connection);
            return NULL;
        }
    
        result->db=fsdb;
        result->u.remote.connection=connection;
        
        if (connection->buf==NULL) {
            connection->buf_size=TSF_DEF_NET_BUF_SIZE;
            connection->buf_cursor=0;
            connection->buf=malloc(connection->buf_size);
            if (connection->buf==NULL) {
                connection_destroy(connection);
                free(result);
                return NULL;
            }
        }
        
        result->u.remote.out_man=
            tsf_serial_out_man_create(network_writer, result);
        if (result->u.remote.out_man==NULL) {
            connection_destroy(connection);
            free(result);
            return NULL;
        }
        
        return result;
    }
#endif
    default: tsf_abort("bad FSDB kind");
    }
}

tsf_fsdb_in_t *tsf_fsdb_get(tsf_fsdb_t *fsdb,
                            tsf_buffer_t *key) {
    if (fsdb==NULL || key==NULL) {
        return NULL;
    }
    
    switch (fsdb->kind) {
    case TSF_FSDB_LOCAL: {
        int fd;
        tsf_fsdb_in_t *result;
        
        if (!tsf_fsdb_guts_get(fsdb,key,&fd,NULL)) {
            return NULL;
        }
    
        result=malloc(sizeof(tsf_fsdb_in_t));
        if (result==NULL) {
            tsf_set_errno("Could not malloc in tsf_fsdb_get");
            close(fd);
            return NULL;
        }
    
        result->db=fsdb;
    
        result->u.local.in=tsf_stream_file_input_fd_open(fd,
                                                         fsdb->limits,
                                                         &fsdb->rdr_attr,
                                                         tsf_true,
                                                         0);
        if (result->u.local.in==NULL) {
            close(fd);
            free(result);
            return NULL;
        }
    
        result->u.local.key=tsf_buffer_dup(key);
        if (result->u.local.key==NULL) {
            tsf_stream_file_input_close(result->u.local.in);
            free(result);
            return NULL;
        }
    
        return result;
    }
#ifdef HAVE_BSDSOCKS
    case TSF_FSDB_REMOTE: {
        tsf_fsdb_connection_t *connection;
        tsf_fsdb_command_t cmd;
        tsf_fsdb_response_t *rsp;
        tsf_fsdb_in_t *result;
        
        cmd.value=tsf_fsdb_command__get;
        cmd.u.get.key=key;
        
        rsp=first_action(fsdb,&connection,&cmd);
        if (rsp==NULL) {
            return NULL;
        }
        
        if (rsp->state.value!=tsf_fsdb_state__get ||
            rsp->payload.value!=tsf_fsdb_response__payload__ok) {
            set_error(rsp);
            if (rsp->payload.value==tsf_fsdb_response__payload__not_found) {
                connection_return(fsdb,connection);
            } else {
                connection_destroy(connection);
            }
            tsf_region_free(rsp);
            return NULL;
        }
        
        tsf_region_free(rsp);
        
        result=malloc(sizeof(tsf_fsdb_in_t));
        if (result==NULL) {
            tsf_set_errno("Could not malloc in tsf_fsdb_get");
            connection_destroy(connection);
            return NULL;
        }
    
        result->db=fsdb;
        result->u.remote.connection=connection;
        
        result->u.remote.rsp=NULL;
        result->u.remote.rsp_cursor=0;
        
        result->u.remote.in_man=
            tsf_serial_in_man_create(network_reader,
                                     result,
                                     fsdb->limits);
        if (result->u.remote.in_man==NULL) {
            connection_destroy(connection);
            free(result);
            return NULL;
        }
        
        return result;
    }
#endif
    default: tsf_abort("bad FSDB kind");
    }
}

tsf_bool_t tsf_fsdb_rm(tsf_fsdb_t *fsdb,
                       tsf_buffer_t *key) {
    if (fsdb==NULL || key==NULL) {
        return tsf_false;
    }
    
    switch (fsdb->kind) {
    case TSF_FSDB_LOCAL:
        return tsf_fsdb_guts_rm(fsdb,key,NULL);
#ifdef HAVE_BSDSOCKS
    case TSF_FSDB_REMOTE: {
        tsf_fsdb_connection_t *connection;
        tsf_fsdb_command_t cmd;
        tsf_fsdb_response_t *rsp;
        
        cmd.value=tsf_fsdb_command__rm;
        cmd.u.rm.key=key;
        
        rsp=first_action(fsdb,&connection,&cmd);
        if (rsp==NULL) {
            return tsf_false;
        }
        
        if (rsp->state.value!=tsf_fsdb_state__clear ||
            rsp->payload.value!=tsf_fsdb_response__payload__ok) {
            set_error(rsp);
            if (rsp->payload.value==tsf_fsdb_response__payload__not_found) {
                connection_return(fsdb,connection);
            } else {
                connection_destroy(connection);
            }
            tsf_region_free(rsp);
            return tsf_false;
        }
        
        tsf_region_free(rsp);
        connection_return(fsdb,connection);
        
        return tsf_true;
    }
#endif
    default: tsf_abort("bad FSDB kind");
    }
}

void tsf_fsdb_in_close(tsf_fsdb_in_t *in) {
    switch (in->db->kind) {
    case TSF_FSDB_LOCAL:
        tsf_stream_file_input_close(in->u.local.in);
        tsf_buffer_destroy(in->u.local.key);
        break;
#ifdef HAVE_BSDSOCKS
    case TSF_FSDB_REMOTE:
        connection_return(in->db,in->u.remote.connection);
        if (in->u.remote.rsp!=NULL) {
            tsf_region_free(in->u.remote.rsp);
        }
        tsf_serial_in_man_destroy(in->u.remote.in_man);
        break;
#endif
    default: tsf_abort("bad FSDB kind");
    }
    free(in);
}

tsf_buffer_t *tsf_fsdb_in_read(tsf_fsdb_in_t *in) {
    switch (in->db->kind) {
    case TSF_FSDB_LOCAL:
        return tsf_stream_file_input_read(in->u.local.in);
#ifdef HAVE_BSDSOCKS
    case TSF_FSDB_REMOTE:
        return tsf_serial_in_man_read(in->u.remote.in_man);
#endif
    default: tsf_abort("bad FSDB kind");
    }
}

tsf_bool_t tsf_fsdb_out_commit(tsf_fsdb_out_t *out) {
    if (out==NULL) {
        return tsf_false;
    }
    
    switch (out->db->kind) {
    case TSF_FSDB_LOCAL:
        if (!tsf_stream_file_output_close(out->u.local.out)) {
            goto abort_local;
        }
        
        if (!tsf_fsdb_guts_commit(out->db,
                                  out->u.local.partname,
                                  out->u.local.key,
                                  NULL)) {
            goto abort_local;
        }
        
        free(out->u.local.partname);
        tsf_buffer_destroy(out->u.local.key);
        free(out);
        
        return tsf_true;
        
    abort_local:
        tsf_fsdb_guts_abort(out->db,out->u.local.partname);
        free(out->u.local.partname);
        tsf_buffer_destroy(out->u.local.key);
        free(out);
        
        return tsf_false;
#ifdef HAVE_BSDSOCKS
    case TSF_FSDB_REMOTE: {
        tsf_fsdb_command_t cmd;
        tsf_fsdb_response_t *rsp;
        
        /* this part could be optimized - we're currently sending the commit command
         * as a separate packet.  it would be best to try to send it as part of the
         * last packet of the flush. */
        
        if (!network_writer_flush(out)) {
            tsf_fsdb_out_abort(out);
            return tsf_false;
        }
        
        cmd.value=tsf_fsdb_command__out_commit;
        if (!tsf_fsdb_command__write(out->u.remote.connection->out,
                                     &cmd) ||
            !tsf_stream_file_output_flush(out->u.remote.connection->out)) {
            tsf_fsdb_out_abort(out);
            return tsf_false;
        }
        
        rsp=connection_read(out->u.remote.connection);
        if (rsp==NULL) {
            tsf_fsdb_out_abort(out);
            return tsf_false;
        }
        
        if (rsp->state.value!=tsf_fsdb_state__clear ||
            rsp->payload.value!=tsf_fsdb_response__payload__ok) {
            set_error(rsp);
            tsf_region_free(rsp);
            tsf_fsdb_out_abort(out);
            return tsf_false;
        }
        
        tsf_region_free(rsp);
        
        connection_return(out->db,out->u.remote.connection);
        tsf_serial_out_man_destroy(out->u.remote.out_man);
        free(out);
        return tsf_true;
    }
#endif
    default: tsf_abort("bad FSDB kind");
    }
}

void tsf_fsdb_out_abort(tsf_fsdb_out_t *out) {
    switch (out->db->kind) {
    case TSF_FSDB_LOCAL:
        tsf_stream_file_output_close(out->u.local.out);
        tsf_fsdb_guts_abort(out->db,out->u.local.partname);
        tsf_buffer_destroy(out->u.local.key);
        free(out->u.local.partname);
        break;
#ifdef HAVE_BSDSOCKS
    case TSF_FSDB_REMOTE:
        connection_destroy(out->u.remote.connection);
        tsf_serial_out_man_destroy(out->u.remote.out_man);
        break;
#endif
    default: tsf_abort("bad FSDB kind");
    }
    free(out);
}

tsf_bool_t tsf_fsdb_out_write(tsf_fsdb_out_t *out,
                              tsf_buffer_t *buf) {
    if (out==NULL || buf==NULL) {
        return tsf_false;
    }
    
    switch (out->db->kind) {
    case TSF_FSDB_LOCAL:
        return tsf_stream_file_output_write(out->u.local.out,buf);
#ifdef HAVE_BSDSOCKS
    case TSF_FSDB_REMOTE:
        return tsf_serial_out_man_write(out->u.remote.out_man,buf);
#endif
    default: tsf_abort("bad FSDB kind");
    }
}


