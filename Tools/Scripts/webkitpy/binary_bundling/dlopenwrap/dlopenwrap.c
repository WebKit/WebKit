/*
 * Copyright (C) 2022 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define dlopen_wrapper_print_error_msg(fmt, ...) \
    do { \
        if (dlopen_wrapper_debug_print >= 1) \
            fprintf(stderr, "[pid=%d] ERROR at dlopenwrap.so: " fmt, getpid(), __VA_ARGS__); \
    } while (0)

#define dlopen_wrapper_print_debug_msg(fmt, ...) \
    do { \
        if (dlopen_wrapper_debug_print >= 2) \
            fprintf(stderr, "[pid=%d] dlopenwrap.so: " fmt, getpid(), __VA_ARGS__); \
    } while (0)

static int dlopen_wrapper_debug_print = -1;
static int dlopen_wrapper_call_real_dlerror = 1;
static char* (*__real_dlerror)(void) = NULL;

// Returns the string "dirname/filename" into the buffer path_joint_dest
void dlopen_wrapper_path_join(const char* dirname, const char* filename, char* path_joint_dest)
{
    path_joint_dest = strncpy(path_joint_dest, dirname, PATH_MAX);
    // add a slash if needed
    if (*(path_joint_dest + strlen(dirname) - 1) != '/') {
        *(path_joint_dest + strlen(dirname)) = '/';
        *(path_joint_dest + strlen(dirname) + 1) = 0;
    }
    strncat(path_joint_dest, filename, PATH_MAX);
}

int is_dir(unsigned char d_type, const char* filepath)
{
    struct stat statbuf;
    // we consider a symbolic link to a dir as a dir, also handle unknown.
    if (d_type == DT_UNKNOWN || d_type == DT_LNK) {
        if (stat(filepath, &statbuf))
            dlopen_wrapper_print_error_msg("Calling stat on \"%s\": %s\n", filepath, strerror(errno));
        return S_ISDIR(statbuf.st_mode);
    }
    return (d_type == DT_DIR);
}

// Recursive function to traverse a directory and obtain the path of the first file named "filename"
// It writes that path to the buffer "candidate_path_dest" if the file is found (return == 1).
// If the file is not found it returns 0 and the contents of "candidate_path_dest" are undefined.
int dlopen_wrapper_recursive_find(const char* filename_to_find, const char* directory_to_search, char* candidate_path_dest)
{

    DIR *dp;
    struct dirent* entry;

    char subdirectory_to_search[PATH_MAX];
    dp = opendir(directory_to_search);

    if (!dp) {
        dlopen_wrapper_print_error_msg("Opening directory \"%s\": %s\n", directory_to_search, strerror(errno));
        return 0;
    }

    while (1) {
        errno = 0;
        entry = readdir(dp);
        if (!entry) {
            if (errno)
                dlopen_wrapper_print_error_msg("Reading directory \"%s\": %s\n", directory_to_search, strerror(errno));
            break;
        }

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        dlopen_wrapper_path_join(directory_to_search, entry->d_name, candidate_path_dest);

        if (is_dir(entry->d_type, candidate_path_dest)) {
            dlopen_wrapper_path_join(directory_to_search, entry->d_name, subdirectory_to_search);
            if (dlopen_wrapper_recursive_find(filename_to_find, subdirectory_to_search, candidate_path_dest)) {
                closedir(dp);
                return 1;
            }
        } else {
            if (!strcmp(filename_to_find, entry->d_name)) {
                if (!access(candidate_path_dest, F_OK | R_OK)) {
                    closedir(dp);
                    return 1;
                }
                dlopen_wrapper_print_error_msg("Access denied to candidate library \"%s\"\n", candidate_path_dest);
            }
        }
    }
    closedir(dp);
    return 0;
}

// Helper function that checks if filename_original_path is a path and is inside libdir.
// If it can't find the file directly by the path, it calls dlopen_wrapper_recursive_find() to search it.
// It writes the path to the buffer "path_found_dest" if the file is found (return == 1).
// If the file is not found it returns 0 and the contents of "path_found_dest" are undefined.
int dlopen_wrapper_get_path_for_file_under_dir(const char* filename_original_path, const char* libdir, char* path_found_dest)
{

    // basename() may modify the buffer passed on the first argument
    char *original_path = strdup(filename_original_path);
    char *base_file_to_dlopen_wrapper_find = basename(original_path);
    int retcode = 1;
    int filename_is_absolute = (*filename_original_path == '/');
    int filename_is_relative = 0;
    if (!filename_is_absolute) {
        for (char* current_char = (char*)filename_original_path; *current_char; ++current_char) {
            if (*current_char == '/') {
                filename_is_relative = 1;
                break;
            }
        }
    }

    if (filename_is_absolute) {
        if (!strncmp(filename_original_path, libdir, strlen(libdir))) {
            if (!access(filename_original_path, F_OK | R_OK)) {
                dlopen_wrapper_print_debug_msg("Filename \"%s\" found under directory \"%s\" without any path rewriting\n", filename_original_path, libdir);
                strncpy(path_found_dest, filename_original_path, PATH_MAX);
                return 1;
            }
        }
        dlopen_wrapper_print_debug_msg("Filename \"%s\" NOT found under directory \"%s\". Will try a search with basename\n", filename_original_path, libdir);
    } else if (filename_is_relative) {
        dlopen_wrapper_path_join(libdir, filename_original_path, path_found_dest);
        if (!access(path_found_dest, F_OK | R_OK)) {
            dlopen_wrapper_print_debug_msg("Filename \"%s\" converted to \"%s\" and found under directory \"%s\"\n", filename_original_path, path_found_dest, libdir);
            return 1;
        }
        dlopen_wrapper_print_debug_msg("Filename \"%s\" NOT found under directory \"%s\"\n", filename_original_path, libdir);
    }

    dlopen_wrapper_print_debug_msg("Trying a search for basename \"%s\" under directory \"%s\"\n", base_file_to_dlopen_wrapper_find, libdir);

    retcode = dlopen_wrapper_recursive_find(base_file_to_dlopen_wrapper_find, libdir, path_found_dest);
    if (original_path)
        free(original_path);
    return retcode;
}

// Helper function to split the paths of ld_library_path and to search for "filename_original_path" inside those paths.
// It will stop searching at the first match.
// It writes the path to the buffer "path_found_dest" if the file is found (return == 1).
// If the file is not found it returns 0 and the contents of "path_found_dest" are undefined.
int dlopen_wrapper_find_library_on_libpath(const char* filename_original_path, const char* ld_library_path, char* path_found_dest)
{

    char *saveptr = NULL;
    char *library_paths = strdup(ld_library_path);
    int found = 0;

    for (char *libdir = strtok_r(library_paths, ":", &saveptr); libdir; libdir = strtok_r(NULL, ":", &saveptr)) {
        // make the path absolute
        char *libdir_rp = realpath(libdir, NULL);
        if (!libdir_rp) {
            dlopen_wrapper_print_error_msg("Calling realpath() on \"%s\": %s\n", libdir, strerror(errno));
            continue;
        }
        dlopen_wrapper_print_debug_msg("Trying search under directory %s\n", libdir);
        if (dlopen_wrapper_get_path_for_file_under_dir(filename_original_path, libdir_rp, path_found_dest)) {
            free(libdir_rp);
            found = 1;
            break;
        }
        free(libdir_rp);
    }
    free(library_paths);
    return found;
}

void *dlopen(const char* filename, int flags)
{
    char new_file_path[PATH_MAX];
    dlopen_wrapper_call_real_dlerror = 1;

    if (dlopen_wrapper_debug_print == -1) {
        char *dlopen_debug_value = getenv("DLOPEN_WRAP_DEBUG");
        if (dlopen_debug_value)
            dlopen_wrapper_debug_print = atoi(dlopen_debug_value);
    }

    static void* (*__real_dlopen)(const char* file, int mode) = NULL;
    if (!__real_dlopen) {
        __real_dlopen = dlsym(RTLD_NEXT, "dlopen");
        if (!__real_dlopen) {
            dlopen_wrapper_print_error_msg("Getting dlopen address: %s\n", dlerror());
            dlopen_wrapper_call_real_dlerror = 0;
            return NULL;
        }
    }
    if (!__real_dlerror) {
        __real_dlerror = dlsym(RTLD_NEXT, "dlerror");
        if (!__real_dlerror) {
            dlopen_wrapper_print_error_msg("Getting dlerror address: %s\n", dlerror());
            return NULL;
        }
    }

    // Don't do anything if filename is NULL or LD_LIBRARY_PATH not defined
    if (!filename)
        return __real_dlopen(filename, flags);

    char *ld_library_path = getenv("LD_LIBRARY_PATH");
    if (!ld_library_path)
        return __real_dlopen(filename, flags);

    if (!dlopen_wrapper_find_library_on_libpath(filename, ld_library_path, new_file_path)) {
        dlopen_wrapper_print_error_msg("Could not find \"%s\" under the directori(es) of LD_LIBRARY_PATH. Refusing to dlopen it\n", filename);
        dlopen_wrapper_call_real_dlerror = 0;
        return NULL;
    }

    if (dlopen_wrapper_debug_print) {
        if (strcmp(filename, new_file_path))
            dlopen_wrapper_print_debug_msg("Original request to dlopen \"%s\" changed to load \"%s\"\n", filename, new_file_path);
        else
            dlopen_wrapper_print_debug_msg("Original request to dlopen \"%s\" allowed (unchanged) because is under the directori(es) of LD_LIBRARY_PATH\n", filename);
    }

    return __real_dlopen(new_file_path, flags);
}

void *dlmopen(Lmid_t lmid, const char* filename, int flags)
{

    char new_file_path[PATH_MAX];
    dlopen_wrapper_call_real_dlerror = 1;

    if (dlopen_wrapper_debug_print == -1) {
        char *dlopen_debug_value = getenv("DLOPEN_WRAP_DEBUG");
        if (dlopen_debug_value)
            dlopen_wrapper_debug_print = atoi(dlopen_debug_value);
    }

    static void* (*__real_dlmopen)(Lmid_t lmidt, const char* file, int mode) = NULL;
    if (!__real_dlmopen) {
        __real_dlmopen = dlsym(RTLD_NEXT, "dlmopen");
        if (!__real_dlmopen) {
            dlopen_wrapper_print_error_msg("Getting dlmopen address: %s\n", dlerror());
            dlopen_wrapper_call_real_dlerror = 0;
            return NULL;
        }
    }
    if (!__real_dlerror) {
        __real_dlerror = dlsym(RTLD_NEXT, "dlerror");
        if (!__real_dlerror) {
            dlopen_wrapper_print_error_msg("Getting dlerror address: %s\n", dlerror());
            return NULL;
        }
    }

    // Don't do anything if filename is NULL or LD_LIBRARY_PATH not defined
    if (!filename)
        return __real_dlmopen(lmid, filename, flags);

    char *ld_library_path = getenv("LD_LIBRARY_PATH");
    if (!ld_library_path)
        return __real_dlmopen(lmid, filename, flags);

    if (!dlopen_wrapper_find_library_on_libpath(filename, ld_library_path, new_file_path)) {
        dlopen_wrapper_print_error_msg("Could not find \"%s\" under the directori(es) of LD_LIBRARY_PATH. Refusing to dlmopen it\n", filename);
        dlopen_wrapper_call_real_dlerror = 0;
        return NULL;
    }

    if (dlopen_wrapper_debug_print) {
        if (strcmp(filename, new_file_path))
            dlopen_wrapper_print_debug_msg("Original request to dlmopen \"%s\" changed to load \"%s\"\n", filename, new_file_path);
        else
            dlopen_wrapper_print_debug_msg("Original request to dlmopen \"%s\" allowed (unchanged) because is under the directori(es) of LD_LIBRARY_PATH\n", filename);
    }

    return __real_dlmopen(lmid, new_file_path, flags);
}


char *dlerror(void)
{
    static char* wrapper_error_msg = "Call blocked by dlopenwrapper library.";

    // __real_dlerror handle was already obtained before on the dlopen() or dlmopen() wrappers.
    // Obtaining it here is not possible because calling dlsym() overwrites the dlerror buffer.
    if (dlopen_wrapper_call_real_dlerror && __real_dlerror)
        return __real_dlerror();

    dlopen_wrapper_call_real_dlerror = 1;
    return wrapper_error_msg;
}
