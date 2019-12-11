/*
 * GAS preprocessor (emulates GNU Assembler's preprocessor)
 *
 *  Copyright (C) 2009 Alexei Svitkine
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
#include <ctype.h>

#include <libyasm.h>
#include "modules/preprocs/gas/gas-eval.h"

#define FALSE 0
#define TRUE  1
#define BSIZE 512

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

typedef struct buffered_line {
    char *line;
    int line_number;
    SLIST_ENTRY(buffered_line) next;
} buffered_line;

typedef struct included_file {
    char *filename;
    int lines_remaining;
    SLIST_ENTRY(included_file) next;
} included_file;

typedef struct macro_entry {
    char *name;
    int num_params;
    char **params;
    int num_lines;
    char **lines;
    STAILQ_ENTRY(macro_entry) next;
} macro_entry;

typedef struct deferred_define {
    char *name;
    char *value;
    SLIST_ENTRY(deferred_define) next;
} deferred_define;

typedef struct expr_state {
    const char *string;
    char *symbol;
    int string_cursor;
} expr_state;

typedef struct yasm_preproc_gas {
    yasm_preproc_base preproc;   /* base structure */

    FILE *in;
    char *in_filename;

    yasm_symtab *defines;
    SLIST_HEAD(deferred_defines_head, deferred_define) deferred_defines;

    int depth;
    int skip_depth;

    int in_comment;

    expr_state expr;

    SLIST_HEAD(buffered_lines_head, buffered_line) buffered_lines;
    SLIST_HEAD(included_files_head, included_file) included_files;
    STAILQ_HEAD(macros_head, macro_entry) macros;

    int in_line_number;
    int next_line_number;
    int current_line_number; /* virtual (output) line number */

    yasm_linemap *cur_lm;
    yasm_errwarns *errwarns;
    int fatal_error;
    int detect_errors_only;
} yasm_preproc_gas;

yasm_preproc_module yasm_gas_LTX_preproc;

/* Forward declarations. */

static int substitute_values(yasm_preproc_gas *pp, char **line_ptr);

/* String helpers. */

static const char *starts_with(const char *big, const char *little)
{
    while (*little) {
        if (*little++ != *big++) {
            return NULL;
        }
    }
    return big;
}

static void skip_whitespace(const char **line)
{
    while (isspace(**line)) {
        (*line)++;
    }
}

static void skip_whitespace2(char **line)
{
    while (isspace(**line)) {
        (*line)++;
    }
}

static const char *matches(const char *line, const char *directive)
{
    skip_whitespace(&line);

    if (*line == '.') {
        line = starts_with(line + 1, directive);
        if (line && (!*line || isspace(*line))) {
            skip_whitespace(&line);
            return line;
        }
    }

    return NULL;
}

static int unquote(const char *arg, char *to, size_t to_size, char q, char expected, const char **remainder)
{
    const char *quote;
    const char *end;
    size_t len;

    skip_whitespace(&arg);
    if (*arg != q) {
        return -1;
    }

    arg++;

    end = arg;
    do {
        quote = strchr(end, q);
        if (!quote) {
            return -2;
        }
        end = quote + 1;
    } while (*(quote - 1) == '\\');

    skip_whitespace(&end);
    if (*end != expected) {
        return -3;
    }

    if (remainder) {
        *remainder = end + 1;
    }
    
    len = (size_t) (quote - arg);
    if (len >= to_size) {
        return -4;
    }

    strncpy(to, arg, len);
    to[len] = '\0';

    return (int) len;
}

/* Line-reading. */

static char *read_line_from_file(yasm_preproc_gas *pp, FILE *file)
{
    int bufsize = BSIZE;
    char *buf;
    char *p;

    buf = yasm_xmalloc((size_t)bufsize);

    /* Loop to ensure entire line is read (don't want to limit line length). */
    p = buf;
    for (;;) {
        if (!fgets(p, bufsize - (p - buf), file)) {
            if (ferror(file)) {
                yasm_error_set(YASM_ERROR_IO, N_("error when reading from file"));
                yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
            }
            break;
        }
        p += strlen(p);
        if (p > buf && p[-1] == '\n') {
            break;
        }
        if ((p - buf) + 1 >= bufsize) {
            /* Increase size of buffer */
            char *oldbuf = buf;
            bufsize *= 2;
            buf = yasm_xrealloc(buf, (size_t) bufsize);
            p = buf + (p - oldbuf);
        }
    }

    if (p == buf) {
        /* No data; must be at EOF */
        yasm_xfree(buf);
        return NULL;
    }

    /* Strip the line ending */
    buf[strcspn(buf, "\r\n")] = '\0';
    return buf;
}

static char *read_line(yasm_preproc_gas *pp)
{
    char *line;

    if (!SLIST_EMPTY(&pp->included_files)) {
        included_file *inc_file = SLIST_FIRST(&pp->included_files);
        if (inc_file->lines_remaining <= 0) {
            SLIST_REMOVE_HEAD(&pp->included_files, next);
            yasm_xfree(inc_file->filename);
            yasm_xfree(inc_file);
        }
    }

    if (!SLIST_EMPTY(&pp->buffered_lines)) {
        buffered_line *bline = SLIST_FIRST(&pp->buffered_lines);
        SLIST_REMOVE_HEAD(&pp->buffered_lines, next);
        line = bline->line;
        if (bline->line_number != -1) {
            pp->next_line_number = bline->line_number;
        }
        yasm_xfree(bline);
        if (!SLIST_EMPTY(&pp->included_files)) {
            SLIST_FIRST(&pp->included_files)->lines_remaining--;
        }
        return line;
    }

    line = read_line_from_file(pp, pp->in);
    if (line) {
        pp->in_line_number++;
        pp->next_line_number = pp->in_line_number;
    }

    return line;
}

static const char *get_arg(yasm_preproc_gas *pp, const char *src, char *dest, size_t dest_size)
{
    const char *comma = strchr(src, ',');
    if (comma) {
        size_t len = (size_t) (comma - src);
        if (len >= dest_size) {
            len = dest_size - 1;
        }
        strncpy(dest, src, len);
        dest[len] = '\0';
        comma++;
        skip_whitespace(&comma);        
    } else {
        yasm_error_set(YASM_ERROR_SYNTAX, N_("expected comma"));
        yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
    }
    return comma;
}

/* GAS expression evaluation. */

static char get_char(yasm_preproc_gas *pp)
{
    return pp->expr.string[pp->expr.string_cursor];
}

static const char *get_str(yasm_preproc_gas *pp)
{
    return pp->expr.string + pp->expr.string_cursor;
}

static void next_char(yasm_preproc_gas *pp)
{
    pp->expr.string_cursor++;
}

static int ishex(char c)
{
    c = tolower(c);
    return isdigit(c) || (c >= 'a' && c <= 'f');
}

static void gas_scan_init(yasm_preproc_gas *pp, struct tokenval *tokval, const char *arg1)
{
    pp->expr.symbol = NULL;
    pp->expr.string = arg1;
    pp->expr.string_cursor = 0;
    memset(tokval, 0, sizeof(struct tokenval));
    tokval->t_type = TOKEN_INVALID;
}

static void gas_scan_cleanup(yasm_preproc_gas *pp, struct tokenval *tokval)
{
    if (tokval->t_integer) {
        yasm_intnum_destroy(tokval->t_integer);
        tokval->t_integer = NULL;
    }
    if (pp->expr.symbol) {
        yasm_xfree(pp->expr.symbol);
        pp->expr.symbol = NULL;
    }
}

static int gas_scan(void *preproc, struct tokenval *tokval)
{
    yasm_preproc_gas *pp = (yasm_preproc_gas *) preproc;
    char c = get_char(pp);
    const char *str;

    tokval->t_charptr = NULL;

    if (c == '\0') {
        return tokval->t_type = TOKEN_EOS;
    }

    if (isspace(c)) {
        do {
            next_char(pp);
            c = get_char(pp);
        } while (isspace(c));
    }

    if (isdigit(c)) {
        int char_index = 0;
        int value = 0;

        do {
            value = value*10 + (c - '0');
            char_index++;
            next_char(pp);
            c = get_char(pp);
            if (char_index == 1 && c == 'x' && value == 0) {
                next_char(pp);
                c = get_char(pp);
                /* Hex notation. */
                while (ishex(c)) {
                    if (isdigit(c)) {
                        value = (value << 4) | (c - '0');
                    } else {
                        value = (value << 4) | (tolower(c) - 'a' + 0xa);
                    }
                    next_char(pp);
                    c = get_char(pp);
                }
                break;
            }
        } while (isdigit(c));

        if (tokval->t_integer) {
            yasm_intnum_destroy(tokval->t_integer);
        }
        tokval->t_integer = yasm_intnum_create_int(value);
        return tokval->t_type = TOKEN_NUM;
    }

    tokval->t_type = TOKEN_INVALID;
    str = get_str(pp);

    {
        /* It should be tested whether GAS supports all of these or if there are missing ones. */
        unsigned i;
        struct {
            const char *op;
            int token;
        } ops[] = {
            { "<<", TOKEN_SHL },
            { ">>", TOKEN_SHR },
            { "//", TOKEN_SDIV },
            { "%%", TOKEN_SMOD },
            { "==", TOKEN_EQ },
            { "!=", TOKEN_NE },
            { "<>", TOKEN_NE },
            { "<>", TOKEN_NE },
            { "<=", TOKEN_LE },
            { ">=", TOKEN_GE },
            { "&&", TOKEN_DBL_AND },
            { "^^", TOKEN_DBL_XOR },
            { "||", TOKEN_DBL_OR }
        };
        for (i = 0; i < sizeof(ops)/sizeof(ops[0]); i++) {
            if (!strcmp(str, ops[i].op)) {
                tokval->t_type = ops[i].token;
                break;
            }
        }
    }

    if (tokval->t_type != TOKEN_INVALID) {
        next_char(pp);
        next_char(pp);
    } else {
        str = get_str(pp);

        next_char(pp);
        tokval->t_type = c;

        /* Is it a symbol? If so we need to make it a TOKEN_ID. */
        if (isalpha(c) || c == '_' || c == '.') {
            int symbol_length = 1;

            c = get_char(pp);
            while (isalnum(c) || c == '$' || c == '_') {
                symbol_length++;
                next_char(pp);
                c = get_char(pp);
            }

            pp->expr.symbol = yasm_xrealloc(pp->expr.symbol, symbol_length + 1);
            memcpy(pp->expr.symbol, str, symbol_length);
            pp->expr.symbol[symbol_length] = '\0';

            tokval->t_type = TOKEN_ID;
            tokval->t_charptr = pp->expr.symbol;
        }
    }
    
    return tokval->t_type;
}

static void gas_err(void *private_data, int severity, const char *fmt, ...)
{
    va_list args;
    yasm_preproc_gas *pp = private_data;

    if (!pp->detect_errors_only) {
        va_start(args, fmt);
        yasm_error_set_va(YASM_ERROR_SYNTAX, N_(fmt), args);
        yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
        va_end(args);
    }

    pp->fatal_error = 1;
}

static long eval_expr(yasm_preproc_gas *pp, const char *arg1)
{
    struct tokenval tv;
    yasm_expr *expr;
    yasm_intnum *intn;
    long value;
    expr_state prev_state;

    if (!*arg1) {
        return 0;
    }

    prev_state = pp->expr;
    gas_scan_init(pp, &tv, arg1);
    expr = evaluate(gas_scan, pp, &tv, pp, CRITICAL, gas_err, pp->defines);
    intn = yasm_expr_get_intnum(&expr, 0);
    value = yasm_intnum_get_int(intn);
    yasm_expr_destroy(expr);
    gas_scan_cleanup(pp, &tv);
    pp->expr = prev_state;

    return value;
}

/* If-directive helpers. */

static int handle_if(yasm_preproc_gas *pp, int is_true)
{
    assert(pp->depth >= 0);
    assert(pp->skip_depth == 0);
    if (is_true) {
        pp->depth++;
    } else {
        pp->skip_depth = 1;
    }
    return 1;
}

static int handle_endif(yasm_preproc_gas *pp)
{
    if (pp->depth) {
        pp->depth--;
    } else {
        yasm_error_set(YASM_ERROR_SYNTAX, N_("\".endif\" without \".if\""));
        yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
        return 0;
    }
    return 1;
}

static int handle_else(yasm_preproc_gas *pp, int is_elseif)
{
    if (!pp->depth) {
        yasm_error_set(YASM_ERROR_SYNTAX, N_("\".%s\" without \".if\""), is_elseif ? "elseif" : "else");
        yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
        return 0;
    } else {
        pp->skip_depth = 1;
    }
    return 1;
}

/* Directive-handling functions. */

static int eval_if(yasm_preproc_gas *pp, int negate, const char *arg1)
{
    long value;
    if (!*arg1) {
        yasm_error_set(YASM_ERROR_SYNTAX, N_("expression is required in \".if\" statement"));
        yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
        return 0;
    }
    value = eval_expr(pp, arg1);
    handle_if(pp, (negate ? !value : !!value));
    return 1;
}

static int eval_else(yasm_preproc_gas *pp, int unused)
{
    return handle_else(pp, 0);
}

static int eval_endif(yasm_preproc_gas *pp, int unused)
{
    return handle_endif(pp);
}

static int eval_elseif(yasm_preproc_gas *pp, int unused, const char *arg1)
{
    if (!*arg1) {
        yasm_error_set(YASM_ERROR_SYNTAX, N_("expression is required in \".elseif\" statement"));
        yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
        return 0;
    }
    if (!handle_else(pp, 1)) {
        return 0;
    }
    return eval_if(pp, 0, arg1);
}

static int eval_ifb(yasm_preproc_gas *pp, int negate, const char *arg1)
{
    int is_blank = !*arg1;
    return handle_if(pp, (negate ? !is_blank : is_blank));
}

static int eval_ifc(yasm_preproc_gas *pp, int negate, const char *args)
{
    char arg1[512], arg2[512];
    const char *remainder;
    int len = unquote(args, arg1, sizeof(arg1), '\'', ',', &remainder);
    if (len >= 0) {
        len = unquote(remainder, arg2, sizeof(arg2), '\'', '\0', NULL);
        if (len >= 0) {
            int result = !strcmp(arg1, arg2);
            return handle_if(pp, (negate ? !result : result));
        }
    } else {
        /* first argument was not single-quoted, assume non-quoted mode */
        remainder = get_arg(pp, args, arg1, sizeof(arg1));
        if (remainder) {
            int result = !strcmp(arg1, remainder);
            return handle_if(pp, (negate ? !result : result));
        }
    }
    yasm_error_set(YASM_ERROR_SYNTAX, N_("\"%s\" expects two single-quoted or unquoted arguments"), negate ? ".ifnc" : ".ifc");
    yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
    return 0;
}

static int eval_ifeqs(yasm_preproc_gas *pp, int negate, const char *args)
{
    char arg1[512], arg2[512];
    const char *remainder;
    int len = unquote(args, arg1, sizeof(arg1), '"', ',', &remainder);
    if (len >= 0) {
        len = unquote(remainder, arg2, sizeof(arg2), '"', '\0', NULL);
        if (len >= 0) {
            int result = !strcmp(arg1, arg2);
            return handle_if(pp, (negate ? !result : result));
        }
    }
    yasm_error_set(YASM_ERROR_SYNTAX, N_("\"%s\" expects two double-quoted arguments"), negate ? ".ifnes" : ".ifeqs");
    yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
    return 1;
}

static int eval_ifdef(yasm_preproc_gas *pp, int negate, const char *name)
{
    yasm_symrec *rec = yasm_symtab_get(pp->defines, name);
    int result = (rec != NULL);
    return handle_if(pp, (negate ? !result : result));
}

static int eval_ifge(yasm_preproc_gas *pp, int negate, const char *arg1)
{
    long value = eval_expr(pp, arg1);
    int result = (value >= 0);
    return handle_if(pp, (negate ? !result : result));
}

static int eval_ifgt(yasm_preproc_gas *pp, int negate, const char *arg1)
{
    long value = eval_expr(pp, arg1);
    int result = (value > 0);
    return handle_if(pp, (negate ? !result : result));
}

static int eval_include(yasm_preproc_gas *pp, int unused, const char *arg1)
{
    char *current_filename;
    char filename[MAXPATHLEN];
    char *line;
    int num_lines;
    FILE *file;
    buffered_line *prev_bline;
    included_file *inc_file;

    if (unquote(arg1, filename, sizeof(filename), '"', '\0', NULL) < 0) {
        yasm_error_set(YASM_ERROR_SYNTAX, N_("string expected"));
        yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
        return 0;
    }

    if (SLIST_EMPTY(&pp->included_files)) {
        current_filename = pp->in_filename;
    } else {
        current_filename = SLIST_FIRST(&pp->included_files)->filename;
    }
    file = yasm_fopen_include(filename, current_filename, "r", NULL);
    if (!file) {
        yasm_error_set(YASM_ERROR_SYNTAX, N_("unable to open included file \"%s\""), filename);
        yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
        return 0;
    }

    num_lines = 0;
    prev_bline = NULL;
    line = read_line_from_file(pp, file);
    while (line) {
        buffered_line *bline = yasm_xmalloc(sizeof(buffered_line));
        bline->line = line;
        bline->line_number = -1;
        if (prev_bline) {
            SLIST_INSERT_AFTER(prev_bline, bline, next);
        } else {
            SLIST_INSERT_HEAD(&pp->buffered_lines, bline, next);
        }
        prev_bline = bline;
        line = read_line_from_file(pp, file);
        num_lines++;
    }

    inc_file = yasm_xmalloc(sizeof(included_file));
    inc_file->filename = yasm__xstrdup(filename);
    inc_file->lines_remaining = num_lines;
    SLIST_INSERT_HEAD(&pp->included_files, inc_file, next);
    return 1;
}

static int try_eval_expr(yasm_preproc_gas *pp, const char *value, long *result)
{
    int success;

    pp->detect_errors_only = 1;
    *result = eval_expr(pp, value);
    success = !pp->fatal_error;
    pp->fatal_error = 0;
    pp->detect_errors_only = 0;

    return success;
}

static int remove_define(yasm_preproc_gas *pp, const char *name, int allow_redefine)
{
    yasm_symrec *rec = yasm_symtab_get(pp->defines, name);
    if (rec) {
        const yasm_symtab_iter *entry;
        yasm_symtab *new_defines;

        if (!allow_redefine) {
            yasm_error_set(YASM_ERROR_SYNTAX, N_("symbol \"%s\" is already defined"), name);
            yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
            return 0;
        }

        new_defines = yasm_symtab_create();

        for (entry = yasm_symtab_first(pp->defines); entry; entry = yasm_symtab_next(entry)) {
            yasm_symrec *entry_rec = yasm_symtab_iter_value(entry);
            const char *rec_name = yasm_symrec_get_name(entry_rec);
            if (strcmp(rec_name, name)) {
                yasm_intnum *num = yasm_intnum_create_int(eval_expr(pp, rec_name));
                yasm_expr *expr = yasm_expr_create_ident(yasm_expr_int(num), 0);
                yasm_symtab_define_equ(new_defines, rec_name, expr, 0);
            } 
        }

        yasm_symtab_destroy(pp->defines);
        pp->defines = new_defines;
    }
    return (rec != NULL);
}

static void add_define(yasm_preproc_gas *pp, const char *name, long value, int allow_redefine, int substitute)
{
    deferred_define *def, *prev_def, *temp_def;
    yasm_intnum *num;
    yasm_expr *expr;

    remove_define(pp, name, allow_redefine);

    /* Add the new define. */
    num = yasm_intnum_create_int(value);
    expr = yasm_expr_create_ident(yasm_expr_int(num), 0);
    yasm_symtab_define_equ(pp->defines, name, expr, 0);

    /* Perform substitution on any deferred defines. */
    if (substitute) {
        prev_def = NULL;
        temp_def = NULL;
        SLIST_FOREACH_SAFE(def, &pp->deferred_defines, next, temp_def) {
            if (substitute_values(pp, &def->value)) {
                /* Value was updated - check if it can be added to the symtab. */
                if (try_eval_expr(pp, def->value, &value)) {
                    add_define(pp, def->name, value, FALSE, FALSE);
                    if (prev_def) {
                        SLIST_NEXT(prev_def, next) = SLIST_NEXT(def, next);
                    } else {
                        SLIST_FIRST(&pp->deferred_defines) = SLIST_NEXT(def, next);
                    }
                    yasm_xfree(def->name);
                    yasm_xfree(def->value);
                    yasm_xfree(def);
                    continue;
                }
            }
            prev_def = def;
        }
    }
}

static int eval_set(yasm_preproc_gas *pp, int allow_redefine, const char *name, const char *value)
{
    if (!pp->skip_depth) {
        long result;

        if (!try_eval_expr(pp, value, &result)) {
            deferred_define *def;
            remove_define(pp, name, allow_redefine);
            def = yasm_xmalloc(sizeof(deferred_define));
            def->name = yasm__xstrdup(name);
            def->value = yasm__xstrdup(value);
            substitute_values(pp, &def->value);
            SLIST_INSERT_HEAD(&pp->deferred_defines, def, next);
        } else {
            add_define(pp, name, result, allow_redefine, TRUE);
        }
    }
    return 1;
}

static int eval_macro(yasm_preproc_gas *pp, int unused, char *args)
{
    char *end;
    char *line;
    long nesting = 1;
    macro_entry *macro = yasm_xmalloc(sizeof(macro_entry));

    memset(macro, 0, sizeof(macro_entry));

    end = args;
    while (*end && !isspace(*end)) {
        end++;
    }
    macro->name = yasm_xmalloc(end - args + 1);
    memcpy(macro->name, args, end - args);
    macro->name[end - args] = '\0';

    skip_whitespace2(&end);
    while (*end) {
        args = end;
        while (*end && !isspace(*end) && *end != ',') {
            end++;
        }
        macro->num_params++;
        macro->params = yasm_xrealloc(macro->params, macro->num_params*sizeof(char *));
        macro->params[macro->num_params - 1] = yasm_xmalloc(end - args + 1);
        memcpy(macro->params[macro->num_params - 1], args, end - args);
        macro->params[macro->num_params - 1][end - args] = '\0';
        skip_whitespace2(&end);
        if (*end == ',') {
            end++;
            skip_whitespace2(&end);
        }
    }

    STAILQ_INSERT_TAIL(&pp->macros, macro, next);

    line = read_line(pp);
    while (line) {
        char *line2 = line;
        skip_whitespace2(&line2);
        if (starts_with(line2, ".macro")) {
            nesting++;
        } else if (starts_with(line, ".endm") && --nesting == 0) {
            return 1;
        }
        macro->num_lines++;
        macro->lines = yasm_xrealloc(macro->lines, macro->num_lines*sizeof(char *));
        macro->lines[macro->num_lines - 1] = line;
        line = read_line(pp);
    }

    yasm_error_set(YASM_ERROR_SYNTAX, N_("unexpected EOF in \".macro\" block"));
    yasm_errwarn_propagate(pp->errwarns, yasm_linemap_get_current(pp->cur_lm));
    return 0;
}

static int eval_endm(yasm_preproc_gas *pp, int unused)
{
    yasm_error_set(YASM_ERROR_SYNTAX, N_("\".endm\" without \".macro\""));
    yasm_errwarn_propagate(pp->errwarns, yasm_linemap_get_current(pp->cur_lm));
    return 0;
}

static void get_param_value(macro_entry *macro, int param_index, const char *args, const char **value, int *length)
{
    int arg_index = 0;
    const char *default_value = NULL;
    const char *end, *eq = strstr(macro->params[param_index], "=");

    if (eq) {
        default_value = eq + 1;
    }

    skip_whitespace(&args);
    end = args;
    while (*end) {
        args = end;
        while (*end && !isspace(*end) && *end != ',') {
            end++;
        }
        if (arg_index == param_index) {
            if (end == args && default_value) {
                *value = default_value;
                *length = strlen(default_value);
            } else {
                *value = args;
                *length = end - args;
            }
            return;
        }
        arg_index++;
        skip_whitespace(&end);
        if (*end == ',') {
            end++;
            skip_whitespace(&end);
        }
    }

    *value = default_value;
    *length = (default_value ? strlen(default_value) : 0);
}

static void expand_macro(yasm_preproc_gas *pp, macro_entry *macro, const char *args)
{
    int i, j;
    buffered_line *prev_bline = NULL;

    for (i = 0; i < macro->num_lines; i++) {
        buffered_line *bline = yasm_xmalloc(sizeof(buffered_line));
        struct tokenval tokval;
        int prev_was_backslash = FALSE;
        int line_length = strlen(macro->lines[i]);
        char *work = yasm__xstrdup(macro->lines[i]);
        expr_state prev_state = pp->expr;

        gas_scan_init(pp, &tokval, work);
        while (gas_scan(pp, &tokval) != TOKEN_EOS) {
            if (prev_was_backslash) {
                if (tokval.t_type == TOKEN_ID) {
                    for (j = 0; j < macro->num_params; j++) {
                        char *end = strstr(macro->params[j], "=");
                        int len = (end ? (size_t)(end - macro->params[j])
                                       : strlen(macro->params[j]));
                        if (!strncmp(tokval.t_charptr, macro->params[j], len)
                            && tokval.t_charptr[len] == '\0') {
                            /* now, find matching argument. */
                            const char *value;
                            char *line = work + (pp->expr.string - work);
                            int cursor = pp->expr.string_cursor;
                            int value_length, delta;

                            get_param_value(macro, j, args, &value, &value_length);

                            len++; /* leading slash */
                            delta = value_length - len;
                            line_length += delta;
                            if (delta > 0) {
                                line = yasm_xrealloc(line, line_length + 1);
                            }
                            memmove(line + cursor - len + value_length, line + cursor, strlen(line + cursor) + 1);
                            memcpy(line + cursor - len, value, value_length);
                            pp->expr.string = work = line;
                            pp->expr.string_cursor += delta;
                            if (pp->expr.symbol) {
                                yasm_xfree(pp->expr.symbol);
                                pp->expr.symbol = NULL;
                            }
                        }
                    }
                }
                prev_was_backslash = FALSE;
            } else if (tokval.t_type == '\\') {
                prev_was_backslash = TRUE;
            }
        }
        gas_scan_cleanup(pp, &tokval);

        bline->line = work + (pp->expr.string - work);
        bline->line_number = -1;
        pp->expr = prev_state;

        if (prev_bline) {
            SLIST_INSERT_AFTER(prev_bline, bline, next);
        } else {
            SLIST_INSERT_HEAD(&pp->buffered_lines, bline, next);
        }
        prev_bline = bline;
    }
}

static int eval_rept(yasm_preproc_gas *pp, int unused, const char *arg1)
{
    long i, n = eval_expr(pp, arg1);
    long num_lines = 0;
    long nesting = 1;
    char *line = read_line(pp);
    buffered_line *prev_bline = NULL;
    SLIST_HEAD(buffered_lines_head, buffered_line) lines;
    int rept_start_file_line_number = pp->next_line_number - 1;
    int rept_start_output_line_number = pp->current_line_number;

    SLIST_INIT(&lines);

    while (line) {
        skip_whitespace2(&line);
        if (starts_with(line, ".rept")) {
            nesting++;
        } else if (starts_with(line, ".endr") && --nesting == 0) {
            for (i = 0; i < n; i++) {
                buffered_line *current_line;
                prev_bline = NULL;
                SLIST_FOREACH(current_line, &lines, next) {
                    buffered_line *bline = yasm_xmalloc(sizeof(buffered_line));
                    bline->line = yasm__xstrdup(current_line->line);
                    bline->line_number = current_line->line_number;
                    if (prev_bline) {
                        SLIST_INSERT_AFTER(prev_bline, bline, next);
                    } else {
                        SLIST_INSERT_HEAD(&pp->buffered_lines, bline, next);
                    }
                    prev_bline = bline;
                }
            }
            if (!SLIST_EMPTY(&pp->included_files)) {
                included_file *inc_file = SLIST_FIRST(&pp->included_files);
                inc_file->lines_remaining += n * num_lines;
            }
            while (!SLIST_EMPTY(&lines)) {
                buffered_line *bline = SLIST_FIRST(&lines);
                SLIST_REMOVE_HEAD(&lines, next);
                yasm_xfree(bline->line);
                yasm_xfree(bline);
            }
            yasm_xfree(line);
            return 1;
        }
        if (n > 0) {
            buffered_line *bline = yasm_xmalloc(sizeof(buffered_line));
            bline->line = line;
            bline->line_number = pp->next_line_number;
            if (prev_bline) {
                SLIST_INSERT_AFTER(prev_bline, bline, next);
            } else {
                SLIST_INSERT_HEAD(&lines, bline, next);
            }
            prev_bline = bline;
        } else {
            yasm_xfree(line);
        }
        line = read_line(pp);
        num_lines++;
    }
    yasm_linemap_set(pp->cur_lm, pp->in_filename, rept_start_output_line_number, rept_start_file_line_number, 0);
    yasm_error_set(YASM_ERROR_SYNTAX, N_("rept without matching endr"));
    yasm_errwarn_propagate(pp->errwarns, rept_start_output_line_number);
    return 0;
}

static int eval_endr(yasm_preproc_gas *pp, int unused)
{
    yasm_error_set(YASM_ERROR_SYNTAX, N_("\".endr\" without \".rept\""));
    yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
    return 0;
}

/* Top-level line processing. */

typedef int (*pp_fn0_t)(yasm_preproc_gas *pp, int param);
typedef int (*pp_fn1_t)(yasm_preproc_gas *pp, int param, const char *arg1);
typedef int (*pp_fn2_t)(yasm_preproc_gas *pp, int param, const char *arg1, const char *arg2);

#define FN(f) ((pp_fn0_t) &(f))

static void kill_comments(yasm_preproc_gas *pp, char *line)
{
    int next = 2;
    char *cstart;

    skip_whitespace2(&line);
    if (*line == '#' || !strncmp(line, "//", 2)) {
        *line = '\0';
        return;
    }

    if (pp->in_comment) {
        cstart = line;
        next = 0;
    } else {
        cstart = strstr(line, "/*");
        next = 2;
    }

    while (cstart) {
        char *cend = strstr(cstart + next, "*/");

        if (!cend) {
            *cstart = '\0';
            pp->in_comment = TRUE;
            return;
        }

        memmove(cstart, cend + 2, strlen(cend + 2) + 1);
        pp->in_comment = FALSE;
        cstart = strstr(cstart, "/*");
        next = 2;
   }
}

static int substitute_values(yasm_preproc_gas *pp, char **line_ptr)
{
    int changed = 0;
    char *line = *line_ptr;
    int line_length = strlen(line);
    struct tokenval tokval;
    expr_state prev_state = pp->expr;

    gas_scan_init(pp, &tokval, line);
    while (gas_scan(pp, &tokval) != TOKEN_EOS) {
        if (tokval.t_type == TOKEN_ID) {
            yasm_symrec *rec = yasm_symtab_get(pp->defines, tokval.t_charptr);
            if (rec) {
                int cursor = pp->expr.string_cursor;
                int len = strlen(tokval.t_charptr);
                char value[64];
                int value_length = sprintf(value, "%ld", eval_expr(pp, tokval.t_charptr));
                int delta = value_length - len;

                line_length += delta;
                if (delta > 0) {
                    line = yasm_xrealloc(line, line_length + 1);
                }
                memmove(line + cursor - len + value_length, line + cursor, strlen(line + cursor) + 1);
                memcpy(line + cursor - len, value, value_length);
                pp->expr.string = line;
                pp->expr.string_cursor = cursor + delta;
                changed = 1;
            }
            yasm_xfree(pp->expr.symbol);
            pp->expr.symbol = NULL;
        }
    }
    gas_scan_cleanup(pp, &tokval);
    pp->expr = prev_state;

    if (changed) {
        *line_ptr = line;
    }

    return changed;
}

static int process_line(yasm_preproc_gas *pp, char **line_ptr)
{
    macro_entry *macro;
    size_t i;
    char *line = *line_ptr;
    struct {
        const char *name;
        int nargs;
        pp_fn0_t fn;
        int param;
    } directives[] = {
        {"else", 0, FN(eval_else), 0},
        {"elseif", 1, FN(eval_elseif), 0},
        {"endif", 0, FN(eval_endif), 0},
        {"if", 1, FN(eval_if), 0},
        {"ifb", 1, FN(eval_ifb), 0},
        {"ifc", 1, FN(eval_ifc), 0},
        {"ifdef", 1, FN(eval_ifdef), 0},
        {"ifeq", 1, FN(eval_if), 1},
        {"ifeqs", 1, FN(eval_ifeqs), 0},
        {"ifge", 1, FN(eval_ifge), 0},
        {"ifgt", 1, FN(eval_ifgt), 0},
        {"ifle", 1, FN(eval_ifgt), 1},
        {"iflt", 1, FN(eval_ifge), 1},
        {"ifnb", 1, FN(eval_ifb), 1},
        {"ifnc", 1, FN(eval_ifc), 1},
        {"ifndef", 1, FN(eval_ifdef), 1},
        {"ifnotdef", 1, FN(eval_ifdef), 1},
        {"ifne", 1, FN(eval_if), 0},
        {"ifnes", 1, FN(eval_ifeqs), 1},
        {"include", 1, FN(eval_include), 0},
        {"set", 2, FN(eval_set), 1},
        {"equ", 2, FN(eval_set), 1},
        {"equiv", 2, FN(eval_set), 0},
        {"macro", 1, FN(eval_macro), 0},
        {"endm", 0, FN(eval_endm), 0},
        {"rept", 1, FN(eval_rept), 0},
        {"endr", 1, FN(eval_endr), 0},
    };

    kill_comments(pp, line);
    skip_whitespace2(&line);
    if (*line == '\0') {
        return FALSE;
    }

    /* See if this is a macro call. */
    STAILQ_FOREACH(macro, &pp->macros, next) {
        const char *remainder = starts_with(line, macro->name);
        if (remainder && (!*remainder || isspace(*remainder))) {
            skip_whitespace2(&line);
            expand_macro(pp, macro, remainder);
            return FALSE;
        }
    }

    for (i = 0; i < sizeof(directives)/sizeof(directives[0]); i++) {
        char buf1[1024];
        const char *remainder = matches(line, directives[i].name);
        
        if (remainder) {
            if (pp->skip_depth) {
                if (!strncmp("if", directives[i].name, 2)) {
                    pp->skip_depth++;
                } else if (!strcmp("endif", directives[i].name)) {
                    pp->skip_depth--;
                } else if (!strcmp("else", directives[i].name)) {
                    if (pp->skip_depth == 1) {
                        pp->skip_depth = 0;
                        pp->depth++;
                    }
                }
                return FALSE;
            } else if (directives[i].nargs == 0) {
                pp_fn0_t fn = (pp_fn0_t) directives[i].fn;
                pp->fatal_error = !fn(pp, directives[i].param);
                return FALSE;
            } else if (directives[i].nargs == 1) {
                pp_fn1_t fn = (pp_fn1_t) directives[i].fn;
                skip_whitespace(&remainder);
                pp->fatal_error = !fn(pp, directives[i].param, remainder);
                return FALSE;
            } else if (directives[i].nargs == 2) {
                remainder = get_arg(pp, remainder, buf1, sizeof(buf1));
                if (!remainder || !*remainder || !*buf1) {
                    yasm_error_set(YASM_ERROR_SYNTAX, N_("\".%s\" expects two arguments"), directives[i].name);
                    yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
                    pp->fatal_error = 1;
                } else {
                    pp_fn2_t fn = (pp_fn2_t) directives[i].fn;
                    pp->fatal_error = !fn(pp, directives[i].param, buf1, remainder);
                }
                return FALSE;
            }
        }
    }

    if (pp->skip_depth == 0) {
        substitute_values(pp, line_ptr);
        return TRUE;
    }

    return FALSE;
}

/* Functions exported by the preprocessor. */

static yasm_preproc *
gas_preproc_create(const char *in_filename, yasm_symtab *symtab,
                   yasm_linemap *lm, yasm_errwarns *errwarns)
{
    FILE *f;
    yasm_preproc_gas *pp = yasm_xmalloc(sizeof(yasm_preproc_gas));

    if (strcmp(in_filename, "-") != 0) {
        f = fopen(in_filename, "r");
        if (!f) {
            yasm__fatal(N_("Could not open input file"));
        }
    } else {
        f = stdin;
    }

    pp->preproc.module = &yasm_gas_LTX_preproc;
    pp->in = f;
    pp->in_filename = yasm__xstrdup(in_filename);
    pp->defines = yasm_symtab_create();
    SLIST_INIT(&pp->deferred_defines);
    yasm_symtab_set_case_sensitive(pp->defines, 1);
    pp->depth = 0;
    pp->skip_depth = 0;
    pp->in_comment = FALSE;
    SLIST_INIT(&pp->buffered_lines);
    SLIST_INIT(&pp->included_files);
    STAILQ_INIT(&pp->macros);
    pp->in_line_number = 0;
    pp->next_line_number = 0;
    pp->current_line_number = 0;
    pp->cur_lm = lm;
    pp->errwarns = errwarns;
    pp->fatal_error = 0;
    pp->detect_errors_only = 0;

    return (yasm_preproc *) pp;
}

static void
gas_preproc_destroy(yasm_preproc *preproc)
{
    yasm_preproc_gas *pp = (yasm_preproc_gas *) preproc;
    yasm_xfree(pp->in_filename);
    yasm_symtab_destroy(pp->defines);
    while (!SLIST_EMPTY(&pp->deferred_defines)) {
        deferred_define *def = SLIST_FIRST(&pp->deferred_defines);
        SLIST_REMOVE_HEAD(&pp->deferred_defines, next);
        yasm_xfree(def->name);
        yasm_xfree(def->value);
        yasm_xfree(def);
    }
    while (!SLIST_EMPTY(&pp->buffered_lines)) {
        buffered_line *bline = SLIST_FIRST(&pp->buffered_lines);
        SLIST_REMOVE_HEAD(&pp->buffered_lines, next);
        yasm_xfree(bline->line);
        yasm_xfree(bline);
    }
    while (!SLIST_EMPTY(&pp->included_files)) {
        included_file *inc_file = SLIST_FIRST(&pp->included_files);
        SLIST_REMOVE_HEAD(&pp->included_files, next);
        yasm_xfree(inc_file->filename);
        yasm_xfree(inc_file);
    }
    while (!STAILQ_EMPTY(&pp->macros)) {
        int i;
        macro_entry *macro = STAILQ_FIRST(&pp->macros);
        STAILQ_REMOVE_HEAD(&pp->macros, next);
        yasm_xfree(macro->name);
        for (i = 0; i < macro->num_params; i++)
            yasm_xfree(macro->params[i]);
        yasm_xfree(macro->params);
        for (i = 0; i < macro->num_lines; i++)
            yasm_xfree(macro->lines[i]);
        yasm_xfree(macro->lines);
        yasm_xfree(macro);
    }
    yasm_xfree(preproc);
}

static char *
gas_preproc_get_line(yasm_preproc *preproc)
{
    yasm_preproc_gas *pp = (yasm_preproc_gas *)preproc;
    int done = FALSE;
    char *line = NULL;

    pp->current_line_number++;

    do {
        if (line != NULL) {
            yasm_xfree(line);
        }
        if (pp->fatal_error) {
            return NULL;
        }
        line = read_line(pp);
        if (line == NULL) {
            if (pp->in_comment) {
                yasm_linemap_set(pp->cur_lm, pp->in_filename, pp->current_line_number, pp->next_line_number, 0);
                yasm_warn_set(YASM_WARN_GENERAL, N_("end of file in comment"));
                yasm_errwarn_propagate(pp->errwarns, pp->current_line_number);
                pp->in_comment = FALSE;
            }
            return NULL;
        }
        done = process_line(pp, &line);
    } while (!done);

    yasm_linemap_set(pp->cur_lm, pp->in_filename, pp->current_line_number, pp->next_line_number, 0);

    return line;
}

static size_t
gas_preproc_get_included_file(yasm_preproc *preproc, char *buf,
                              size_t max_size)
{
    /* TODO */
    return 0;
}

static void
gas_preproc_add_include_file(yasm_preproc *preproc, const char *filename)
{
    yasm_preproc_gas *pp = (yasm_preproc_gas *) preproc;
    eval_include(pp, 0, filename);
}

static void
gas_preproc_predefine_macro(yasm_preproc *preproc, const char *macronameval)
{
    yasm_preproc_gas *pp = (yasm_preproc_gas *) preproc;
    const char *eq = strstr(macronameval, "=");
    char *name, *value;
    if (eq) {
        value = yasm__xstrdup(eq + 1);
        name = yasm_xmalloc(eq - macronameval + 1);
        memcpy(name, macronameval, eq - macronameval);
        name[eq - macronameval] = '\0';
    } else {
        name = yasm__xstrdup(macronameval);
        value = yasm__xstrdup("");
    }
    eval_set(pp, 1, name, value);
    yasm_xfree(name);
    yasm_xfree(value);
}

static void
gas_preproc_undefine_macro(yasm_preproc *preproc, const char *macroname)
{
    /* TODO */
}

static void
gas_preproc_define_builtin(yasm_preproc *preproc, const char *macronameval)
{
    /* TODO */
}

static void
gas_preproc_add_standard(yasm_preproc *preproc, const char **macros)
{
    /* TODO */
}


/* Define preproc structure -- see preproc.h for details */
yasm_preproc_module yasm_gas_LTX_preproc = {
    "GNU AS (GAS)-compatible preprocessor",
    "gas",
    gas_preproc_create,
    gas_preproc_destroy,
    gas_preproc_get_line,
    gas_preproc_get_included_file,
    gas_preproc_add_include_file,
    gas_preproc_predefine_macro,
    gas_preproc_undefine_macro,
    gas_preproc_define_builtin,
    gas_preproc_add_standard
};
