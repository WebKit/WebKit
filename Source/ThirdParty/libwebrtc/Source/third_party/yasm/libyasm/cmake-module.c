/*
 * YASM module loader
 *
 *  Copyright (C) 2004-2007  Peter Johnson
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <util.h>

#include <libyasm.h>


typedef struct loaded_module {
    const char *keyword;            /* module keyword */
    void *data;                     /* associated data */
} loaded_module;

static HAMT *loaded_modules[] = {NULL, NULL, NULL, NULL, NULL, NULL};

static void
load_module_destroy(/*@only@*/ void *data)
{
    /* do nothing */
}

void *
yasm_load_module(yasm_module_type type, const char *keyword)
{
    if (!loaded_modules[type])
        return NULL;
    return HAMT_search(loaded_modules[type], keyword);
}

void
yasm_register_module(yasm_module_type type, const char *keyword, void *data)
{
    int replace = 1;

    assert(type < sizeof(loaded_modules));

    if (!loaded_modules[type])
        loaded_modules[type] = HAMT_create(0, yasm_internal_error_);

    HAMT_insert(loaded_modules[type], keyword, data, &replace,
                load_module_destroy);
}

typedef struct {
    yasm_module_type type;
    void (*printfunc) (const char *name, const char *keyword);
} list_one_data;

static int
yasm_list_one_module(void *node, void *d)
{
    list_one_data *data = (list_one_data *)d;
    yasm_arch_module *arch;
    yasm_dbgfmt_module *dbgfmt;
    yasm_objfmt_module *objfmt;
    yasm_listfmt_module *listfmt;
    yasm_parser_module *parser;
    yasm_preproc_module *preproc;

    switch (data->type) {
        case YASM_MODULE_ARCH:
            arch = node;
            data->printfunc(arch->name, arch->keyword);
            break;
        case YASM_MODULE_DBGFMT:
            dbgfmt = node;
            data->printfunc(dbgfmt->name, dbgfmt->keyword);
            break;
        case YASM_MODULE_OBJFMT:
            objfmt = node;
            data->printfunc(objfmt->name, objfmt->keyword);
            break;
        case YASM_MODULE_LISTFMT:
            listfmt = node;
            data->printfunc(listfmt->name, listfmt->keyword);
            break;
        case YASM_MODULE_PARSER:
            parser = node;
            data->printfunc(parser->name, parser->keyword);
            break;
        case YASM_MODULE_PREPROC:
            preproc = node;
            data->printfunc(preproc->name, preproc->keyword);
            break;
    }
    return 0;
}

void
yasm_list_modules(yasm_module_type type,
                  void (*printfunc) (const char *name, const char *keyword))
{
    list_one_data data;

    /* Go through available list, and try to load each one */
    if (!loaded_modules[type])
        return;

    data.type = type;
    data.printfunc = printfunc;

    HAMT_traverse(loaded_modules[type], &data, yasm_list_one_module);
}
