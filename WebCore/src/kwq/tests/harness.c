/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define LIST "test.list"
#define BUFSIZE 1024
#define MAX_NAME_LEN 40
#define TEST_ERR_LEN 29
#define CHK_ERR_LEN 28

int ok = 0;
int no = 0;

void ppad(int pad) {
    if (pad > 0) {
        while (--pad) {
            printf(".");
        }
    }
}

void *emalloc(size_t size) {
     void *p;
     
     p = malloc(size);
     
     if (!p) {
          fprintf(stderr, "memory allocation failure\n");
          exit(1);
     }
     
     return p;
}

int getfile(const char *filename, char **text) {
    FILE *file = NULL;
    int total_read = 0;

    file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, strerror(errno), filename);
        return -1;
    }
    else {
        int char_size = sizeof(char);
        int index = 0;
        int cap = BUFSIZE;
        char *buf;

        *text = emalloc(cap);
        buf = emalloc(BUFSIZE);
    
        do {
            int read;
            read = fread(buf, char_size, BUFSIZE, file);
            if (ferror(file) != 0) {
                free(buf);
                free(*text);
                fclose(file);
                fprintf(stderr, "io error: %d\n", __LINE__);
                exit(1);
            }
            total_read += read;
            if (total_read > cap) {
                char *newmem = realloc(*text, (cap * 2));
                if(newmem == NULL) {
                    free(buf);
                    free(*text);
                    fprintf(stderr, "io error: %d\n", __LINE__);
                    exit(1);
                }
                *text = newmem;    
                cap = total_read;
            }
            memcpy((*text) + index, buf, read);
            index = total_read;
        }
        while (feof(file) == 0);
        free(buf);
        (*text)[total_read] = '\0';
        /* close file */
        if (fclose(file) != 0) {
            fprintf(stderr, "io error: %d\n", __LINE__);
            exit(1);
        }
    }
    return total_read;
}

int runtest(const char *test) {
    int pid;
    int fd;
    char chkname[64];
    char outname[64];
    char *out;
    char *chk;
    int outlen;
    int chklen;
    struct stat buf;
    int pad;
    int result;

    result = 0;

    strcpy(chkname, test);
    strcat(chkname, ".chk");

    strcpy(outname, test);
    strcat(outname, ".out");

    printf("%s", test);
    
    if(stat(test, &buf)) {
        pad = TEST_ERR_LEN - strlen(test);
        ppad(pad);
        printf("test file not found\n");
        return -1;    
    }

    if(stat(chkname, &buf)) {
        pad = CHK_ERR_LEN - strlen(test);
        ppad(pad);
        printf("check file not found\n");
        return -1;    
    }

    /* pad with dots */
    pad = MAX_NAME_LEN - strlen(test);
    ppad(pad);

    /* Fork a child to run the test */
    pid = fork();
    if (pid == 0) {     /* child */

        /* Create a temporary file to catch output of test program */
        fd = open(outname, O_CREAT | O_WRONLY, S_IRWXU);
        if (fd == -1) {
            fprintf(stderr, "open of outfile failed\n");
        }
        else {
            /* Redirect stdout to a temporary file */
            close(1);
            dup(fd);
            close(fd);

            /* exec test file */
            result = execl(test, test, NULL);

            fprintf(stderr, "exec of %s failed\n", test);
            exit(1);
        }
    } 
    else if (pid == -1) {
        fprintf(stderr, "fork failed\n");
        exit(1);
    }

    /* Wait for child to complete before returning */
    while (wait(NULL) != pid) {}  /* empty loop */

    /* collect result and check output from files */    
    outlen = getfile(outname, &out);
    chklen = getfile(chkname, &chk);
        
    /* compare output with expected result */
    if (result != 0) {
        no++;
        printf("fail [exit code]\n");
    }
    else if (outlen != chklen) {
        no++;
        printf("fail [!= lengths]\n");
    }
    else if (memcmp(out, chk, chklen) != 0) {
        no++;
         printf("fail [!= output]\n");
   } 
    else {
        ok++;
        printf(".ok\n");
    }

    free(out);
    free(chk);

    /* clean up the temporary file */
    remove(outname);

    return 0;
}

int main(int argc, char **argv) {
    
    char **suites;
    char *list;
    char *line;
    int num;
    int t;
    int i;
    int head;

    printf("harness begin\n");
    printf("=========================================================\n");

    if (argc < 2) {
        suites = emalloc(sizeof(char *));
        suites[0] = LIST;
        num = 1;
    }
    else {
        num = argc - 1;
        suites = emalloc(num * sizeof(char *));
        for (i = 0; i < num; i++) {
            suites[i] = argv[i + 1];
        }
    }

    head = 1;
    for (i = 0; i < num; i++) {
        if (getfile(suites[i], &list) == -1) {
          continue;
        }
        line = strtok(list, "\n");
        while (line != NULL) {
            if (line[0] == '#') {
                head = 0;
                printf("%s\n", line);
            }
            else {
                if (head == 0) {
                    printf("Test Name                             Output\n");
                    printf("---------------------------------------------------------\n");
                }
                head = 1;
                runtest(line);
            }
            line = strtok(NULL, "\n");
        }
        free(list);
    }
    free(suites);

    t = ok + no;
    printf("---------------------------------------------------------\n");
    printf("Total Tests Run:  %4d\n", t);
    printf("---------------------------------------------------------\n");
    printf("Summary            Output\n");
    printf("Passed:      %4d (%6.2f%%)\n", 
             ok, ((float)ok / (float)t * 100)
             );
    printf("Failed:      %4d (%6.2f%%)\n", 
             no, ((float)no / (float)t * 100)
             );
    printf("=========================================================\n");
    printf("harness done\n");

    return no;

}

/*=============================================================================
// end of file: $RCSfile$
==============================================================================*/

