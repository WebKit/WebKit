/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "common/args_helper.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define SET_ERR_STRING(...) \
  if (err_msg) snprintf(err_msg, ARG_ERR_MSG_MAX_LEN, __VA_ARGS__)

struct arg arg_init(char **argv) {
  struct arg a;

  a.argv = argv;
  a.argv_step = 1;
  a.name = NULL;
  a.val = NULL;
  a.def = NULL;
  return a;
}

int arg_match_helper(struct arg *arg_, const struct arg_def *def, char **argv,
                     char *err_msg) {
  struct arg arg;

  if (err_msg) err_msg[0] = '\0';

  assert(def->has_val == 0 || def->has_val == 1 || def->has_val == -1);

  if (!argv[0] || argv[0][0] != '-') return 0;

  arg = arg_init(argv);

  if (def->short_name && !strcmp(arg.argv[0] + 1, def->short_name)) {
    arg.name = arg.argv[0] + 1;
    arg.val = def->has_val ? arg.argv[1] : NULL;
    arg.argv_step = def->has_val ? 2 : 1;
  } else if (def->long_name) {
    const size_t name_len = strlen(def->long_name);

    if (arg.argv[0][1] == '-' &&
        !strncmp(arg.argv[0] + 2, def->long_name, name_len) &&
        (arg.argv[0][name_len + 2] == '=' ||
         arg.argv[0][name_len + 2] == '\0')) {
      arg.name = arg.argv[0] + 2;
      arg.val = arg.name[name_len] == '=' ? arg.name + name_len + 1 : NULL;
      arg.argv_step = 1;
    }
  }

  if (arg.name) {
    if (def->has_val == -1) {
      arg.def = def;
      *arg_ = arg;
      return 1;
    }

    if (!arg.val && def->has_val) {
      SET_ERR_STRING("Error: option %s requires argument.\n", arg.name);
      return 0;
    }

    if (arg.val && !def->has_val) {
      SET_ERR_STRING("Error: option %s requires no argument.\n", arg.name);
      return 0;
    }

    arg.def = def;
    *arg_ = arg;
    return 1;
  }

  return 0;
}

unsigned int arg_parse_uint_helper(const struct arg *arg, char *err_msg) {
  char *endptr;
  const unsigned long rawval = strtoul(arg->val, &endptr, 10);  // NOLINT

  if (err_msg) err_msg[0] = '\0';

  if (arg->val[0] != '\0' && endptr[0] == '\0') {
    if (rawval <= UINT_MAX) return (unsigned int)rawval;
    SET_ERR_STRING("Option %s: Value %lu out of range for unsigned int\n",
                   arg->name, rawval);
    return 0;
  }
  SET_ERR_STRING("Option %s: Invalid character '%c'\n", arg->name, *endptr);
  return 0;
}

int arg_parse_int_helper(const struct arg *arg, char *err_msg) {
  char *endptr;
  const long rawval = strtol(arg->val, &endptr, 10);  // NOLINT

  if (err_msg) err_msg[0] = '\0';

  if (arg->val[0] != '\0' && endptr[0] == '\0') {
    if (rawval >= INT_MIN && rawval <= INT_MAX) return (int)rawval;
    SET_ERR_STRING("Option %s: Value %ld out of range for signed int\n",
                   arg->name, rawval);
    return 0;
  }
  SET_ERR_STRING("Option %s: Invalid character '%c'\n", arg->name, *endptr);
  return 0;
}

struct aom_rational arg_parse_rational_helper(const struct arg *arg,
                                              char *err_msg) {
  long rawval;  // NOLINT
  char *endptr;
  struct aom_rational rat = { 0, 1 };

  if (err_msg) err_msg[0] = '\0';

  /* parse numerator */
  rawval = strtol(arg->val, &endptr, 10);

  if (arg->val[0] != '\0' && endptr[0] == '/') {
    if (rawval >= INT_MIN && rawval <= INT_MAX) {
      rat.num = (int)rawval;
    } else {
      SET_ERR_STRING("Option %s: Value %ld out of range for signed int\n",
                     arg->name, rawval);
      return rat;
    }
  } else {
    SET_ERR_STRING("Option %s: Expected / at '%c'\n", arg->name, *endptr);
    return rat;
  }

  /* parse denominator */
  rawval = strtol(endptr + 1, &endptr, 10);

  if (arg->val[0] != '\0' && endptr[0] == '\0') {
    if (rawval >= INT_MIN && rawval <= INT_MAX) {
      rat.den = (int)rawval;
    } else {
      SET_ERR_STRING("Option %s: Value %ld out of range for signed int\n",
                     arg->name, rawval);
      return rat;
    }
  } else {
    SET_ERR_STRING("Option %s: Invalid character '%c'\n", arg->name, *endptr);
    return rat;
  }

  return rat;
}

int arg_parse_enum_helper(const struct arg *arg, char *err_msg) {
  const struct arg_enum_list *listptr;
  long rawval;  // NOLINT
  char *endptr;

  if (err_msg) err_msg[0] = '\0';

  /* First see if the value can be parsed as a raw value */
  rawval = strtol(arg->val, &endptr, 10);
  if (arg->val[0] != '\0' && endptr[0] == '\0') {
    /* Got a raw value, make sure it's valid */
    for (listptr = arg->def->enums; listptr->name; listptr++)
      if (listptr->val == rawval) return (int)rawval;
  }

  /* Next see if it can be parsed as a string */
  for (listptr = arg->def->enums; listptr->name; listptr++)
    if (!strcmp(arg->val, listptr->name)) return listptr->val;

  SET_ERR_STRING("Option %s: Invalid value '%s'\n", arg->name, arg->val);
  return 0;
}

int arg_parse_enum_or_int_helper(const struct arg *arg, char *err_msg) {
  if (arg->def->enums) return arg_parse_enum_helper(arg, err_msg);
  return arg_parse_int_helper(arg, err_msg);
}

// parse a comma separated list of at most n integers
// return the number of elements in the list
int arg_parse_list_helper(const struct arg *arg, int *list, int n,
                          char *err_msg) {
  const char *ptr = arg->val;
  char *endptr;
  int i = 0;

  if (err_msg) err_msg[0] = '\0';

  while (ptr[0] != '\0') {
    long rawval = strtol(ptr, &endptr, 10);  // NOLINT
    if (rawval < INT_MIN || rawval > INT_MAX) {
      SET_ERR_STRING("Option %s: Value %ld out of range for signed int\n",
                     arg->name, rawval);
      return 0;
    } else if (i >= n) {
      SET_ERR_STRING("Option %s: List has more than %d entries\n", arg->name,
                     n);
      return 0;
    } else if (*endptr == ',') {
      endptr++;
    } else if (*endptr != '\0') {
      SET_ERR_STRING("Option %s: Bad list separator '%c'\n", arg->name,
                     *endptr);
      return 0;
    }
    list[i++] = (int)rawval;
    ptr = endptr;
  }
  return i;
}
