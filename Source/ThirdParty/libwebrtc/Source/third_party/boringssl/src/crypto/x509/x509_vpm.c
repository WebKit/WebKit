/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2004.
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#include <string.h>

#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/stack.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "../internal.h"
#include "../x509v3/internal.h"
#include "internal.h"


// X509_VERIFY_PARAM functions

#define SET_HOST 0
#define ADD_HOST 1

static void str_free(char *s) { OPENSSL_free(s); }

#define string_stack_free(sk) sk_OPENSSL_STRING_pop_free(sk, str_free)

static int int_x509_param_set_hosts(X509_VERIFY_PARAM *param, int mode,
                                    const char *name, size_t namelen) {
  char *copy;

  if (name == NULL || namelen == 0) {
    // Unlike OpenSSL, we reject trying to set or add an empty name.
    return 0;
  }

  // Refuse names with embedded NUL bytes.
  // XXX: Do we need to push an error onto the error stack?
  if (name && OPENSSL_memchr(name, '\0', namelen)) {
    return 0;
  }

  if (mode == SET_HOST && param->hosts) {
    string_stack_free(param->hosts);
    param->hosts = NULL;
  }

  copy = OPENSSL_strndup(name, namelen);
  if (copy == NULL) {
    return 0;
  }

  if (param->hosts == NULL &&
      (param->hosts = sk_OPENSSL_STRING_new_null()) == NULL) {
    OPENSSL_free(copy);
    return 0;
  }

  if (!sk_OPENSSL_STRING_push(param->hosts, copy)) {
    OPENSSL_free(copy);
    if (sk_OPENSSL_STRING_num(param->hosts) == 0) {
      sk_OPENSSL_STRING_free(param->hosts);
      param->hosts = NULL;
    }
    return 0;
  }

  return 1;
}

static void x509_verify_param_zero(X509_VERIFY_PARAM *param) {
  if (!param) {
    return;
  }
  param->name = NULL;
  param->purpose = 0;
  param->trust = 0;
  // param->inh_flags = X509_VP_FLAG_DEFAULT;
  param->inh_flags = 0;
  param->flags = 0;
  param->depth = -1;
  if (param->policies) {
    sk_ASN1_OBJECT_pop_free(param->policies, ASN1_OBJECT_free);
    param->policies = NULL;
  }
  if (param->hosts) {
    string_stack_free(param->hosts);
    param->hosts = NULL;
  }
  if (param->peername) {
    OPENSSL_free(param->peername);
    param->peername = NULL;
  }
  if (param->email) {
    OPENSSL_free(param->email);
    param->email = NULL;
    param->emaillen = 0;
  }
  if (param->ip) {
    OPENSSL_free(param->ip);
    param->ip = NULL;
    param->iplen = 0;
  }
  param->poison = 0;
}

X509_VERIFY_PARAM *X509_VERIFY_PARAM_new(void) {
  X509_VERIFY_PARAM *param;
  param = OPENSSL_malloc(sizeof(X509_VERIFY_PARAM));
  if (!param) {
    return NULL;
  }
  OPENSSL_memset(param, 0, sizeof(X509_VERIFY_PARAM));
  x509_verify_param_zero(param);
  return param;
}

void X509_VERIFY_PARAM_free(X509_VERIFY_PARAM *param) {
  if (param == NULL) {
    return;
  }
  x509_verify_param_zero(param);
  OPENSSL_free(param);
}

//-
// This function determines how parameters are "inherited" from one structure
// to another. There are several different ways this can happen.
//
// 1. If a child structure needs to have its values initialized from a parent
//    they are simply copied across. For example SSL_CTX copied to SSL.
// 2. If the structure should take on values only if they are currently unset.
//    For example the values in an SSL structure will take appropriate value
//    for SSL servers or clients but only if the application has not set new
//    ones.
//
// The "inh_flags" field determines how this function behaves.
//
// Normally any values which are set in the default are not copied from the
// destination and verify flags are ORed together.
//
// If X509_VP_FLAG_DEFAULT is set then anything set in the source is copied
// to the destination. Effectively the values in "to" become default values
// which will be used only if nothing new is set in "from".
//
// If X509_VP_FLAG_OVERWRITE is set then all value are copied across whether
// they are set or not. Flags is still Ored though.
//
// If X509_VP_FLAG_RESET_FLAGS is set then the flags value is copied instead
// of ORed.
//
// If X509_VP_FLAG_LOCKED is set then no values are copied.
//
// If X509_VP_FLAG_ONCE is set then the current inh_flags setting is zeroed
// after the next call.

// Macro to test if a field should be copied from src to dest

#define test_x509_verify_param_copy(field, def) \
  (to_overwrite ||                              \
   ((src->field != (def)) && (to_default || (dest->field == (def)))))

// Macro to test and copy a field if necessary

#define x509_verify_param_copy(field, def)     \
  if (test_x509_verify_param_copy(field, def)) \
  dest->field = src->field

int X509_VERIFY_PARAM_inherit(X509_VERIFY_PARAM *dest,
                              const X509_VERIFY_PARAM *src) {
  unsigned long inh_flags;
  int to_default, to_overwrite;
  if (!src) {
    return 1;
  }
  inh_flags = dest->inh_flags | src->inh_flags;

  if (inh_flags & X509_VP_FLAG_ONCE) {
    dest->inh_flags = 0;
  }

  if (inh_flags & X509_VP_FLAG_LOCKED) {
    return 1;
  }

  if (inh_flags & X509_VP_FLAG_DEFAULT) {
    to_default = 1;
  } else {
    to_default = 0;
  }

  if (inh_flags & X509_VP_FLAG_OVERWRITE) {
    to_overwrite = 1;
  } else {
    to_overwrite = 0;
  }

  x509_verify_param_copy(purpose, 0);
  x509_verify_param_copy(trust, 0);
  x509_verify_param_copy(depth, -1);

  // If overwrite or check time not set, copy across

  if (to_overwrite || !(dest->flags & X509_V_FLAG_USE_CHECK_TIME)) {
    dest->check_time = src->check_time;
    dest->flags &= ~X509_V_FLAG_USE_CHECK_TIME;
    // Don't need to copy flag: that is done below
  }

  if (inh_flags & X509_VP_FLAG_RESET_FLAGS) {
    dest->flags = 0;
  }

  dest->flags |= src->flags;

  if (test_x509_verify_param_copy(policies, NULL)) {
    if (!X509_VERIFY_PARAM_set1_policies(dest, src->policies)) {
      return 0;
    }
  }

  // Copy the host flags if and only if we're copying the host list
  if (test_x509_verify_param_copy(hosts, NULL)) {
    if (dest->hosts) {
      string_stack_free(dest->hosts);
      dest->hosts = NULL;
    }
    if (src->hosts) {
      dest->hosts =
          sk_OPENSSL_STRING_deep_copy(src->hosts, OPENSSL_strdup, str_free);
      if (dest->hosts == NULL) {
        return 0;
      }
      dest->hostflags = src->hostflags;
    }
  }

  if (test_x509_verify_param_copy(email, NULL)) {
    if (!X509_VERIFY_PARAM_set1_email(dest, src->email, src->emaillen)) {
      return 0;
    }
  }

  if (test_x509_verify_param_copy(ip, NULL)) {
    if (!X509_VERIFY_PARAM_set1_ip(dest, src->ip, src->iplen)) {
      return 0;
    }
  }

  dest->poison = src->poison;

  return 1;
}

int X509_VERIFY_PARAM_set1(X509_VERIFY_PARAM *to,
                           const X509_VERIFY_PARAM *from) {
  unsigned long save_flags = to->inh_flags;
  int ret;
  to->inh_flags |= X509_VP_FLAG_DEFAULT;
  ret = X509_VERIFY_PARAM_inherit(to, from);
  to->inh_flags = save_flags;
  return ret;
}

static int int_x509_param_set1(char **pdest, size_t *pdestlen, const char *src,
                               size_t srclen) {
  void *tmp;
  if (src == NULL || srclen == 0) {
    // Unlike OpenSSL, we do not allow an empty string to disable previously
    // configured checks.
    return 0;
  }

  tmp = OPENSSL_memdup(src, srclen);
  if (!tmp) {
    return 0;
  }

  if (*pdest) {
    OPENSSL_free(*pdest);
  }
  *pdest = tmp;
  if (pdestlen) {
    *pdestlen = srclen;
  }
  return 1;
}

int X509_VERIFY_PARAM_set1_name(X509_VERIFY_PARAM *param, const char *name) {
  if (param->name) {
    OPENSSL_free(param->name);
  }
  param->name = OPENSSL_strdup(name);
  if (param->name) {
    return 1;
  }
  return 0;
}

int X509_VERIFY_PARAM_set_flags(X509_VERIFY_PARAM *param, unsigned long flags) {
  param->flags |= flags;
  return 1;
}

int X509_VERIFY_PARAM_clear_flags(X509_VERIFY_PARAM *param,
                                  unsigned long flags) {
  param->flags &= ~flags;
  return 1;
}

unsigned long X509_VERIFY_PARAM_get_flags(X509_VERIFY_PARAM *param) {
  return param->flags;
}

int X509_VERIFY_PARAM_set_purpose(X509_VERIFY_PARAM *param, int purpose) {
  return X509_PURPOSE_set(&param->purpose, purpose);
}

int X509_VERIFY_PARAM_set_trust(X509_VERIFY_PARAM *param, int trust) {
  return X509_TRUST_set(&param->trust, trust);
}

void X509_VERIFY_PARAM_set_depth(X509_VERIFY_PARAM *param, int depth) {
  param->depth = depth;
}

void X509_VERIFY_PARAM_set_time_posix(X509_VERIFY_PARAM *param, int64_t t) {
  param->check_time = t;
  param->flags |= X509_V_FLAG_USE_CHECK_TIME;
}

void X509_VERIFY_PARAM_set_time(X509_VERIFY_PARAM *param, time_t t) {
  X509_VERIFY_PARAM_set_time_posix(param, t);
}

int X509_VERIFY_PARAM_add0_policy(X509_VERIFY_PARAM *param,
                                  ASN1_OBJECT *policy) {
  if (!param->policies) {
    param->policies = sk_ASN1_OBJECT_new_null();
    if (!param->policies) {
      return 0;
    }
  }
  if (!sk_ASN1_OBJECT_push(param->policies, policy)) {
    return 0;
  }
  return 1;
}

int X509_VERIFY_PARAM_set1_policies(X509_VERIFY_PARAM *param,
                                    const STACK_OF(ASN1_OBJECT) *policies) {
  if (!param) {
    return 0;
  }

  sk_ASN1_OBJECT_pop_free(param->policies, ASN1_OBJECT_free);
  if (!policies) {
    param->policies = NULL;
    return 1;
  }

  param->policies =
      sk_ASN1_OBJECT_deep_copy(policies, OBJ_dup, ASN1_OBJECT_free);
  if (!param->policies) {
    return 0;
  }

  return 1;
}

int X509_VERIFY_PARAM_set1_host(X509_VERIFY_PARAM *param, const char *name,
                                size_t namelen) {
  if (!int_x509_param_set_hosts(param, SET_HOST, name, namelen)) {
    param->poison = 1;
    return 0;
  }
  return 1;
}

int X509_VERIFY_PARAM_add1_host(X509_VERIFY_PARAM *param, const char *name,
                                size_t namelen) {
  if (!int_x509_param_set_hosts(param, ADD_HOST, name, namelen)) {
    param->poison = 1;
    return 0;
  }
  return 1;
}

void X509_VERIFY_PARAM_set_hostflags(X509_VERIFY_PARAM *param,
                                     unsigned int flags) {
  param->hostflags = flags;
}

char *X509_VERIFY_PARAM_get0_peername(X509_VERIFY_PARAM *param) {
  return param->peername;
}

int X509_VERIFY_PARAM_set1_email(X509_VERIFY_PARAM *param, const char *email,
                                 size_t emaillen) {
  if (OPENSSL_memchr(email, '\0', emaillen) != NULL ||
      !int_x509_param_set1(&param->email, &param->emaillen, email, emaillen)) {
    param->poison = 1;
    return 0;
  }

  return 1;
}

int X509_VERIFY_PARAM_set1_ip(X509_VERIFY_PARAM *param, const unsigned char *ip,
                              size_t iplen) {
  if ((iplen != 4 && iplen != 16) ||
      !int_x509_param_set1((char **)&param->ip, &param->iplen, (char *)ip,
                           iplen)) {
    param->poison = 1;
    return 0;
  }

  return 1;
}

int X509_VERIFY_PARAM_set1_ip_asc(X509_VERIFY_PARAM *param, const char *ipasc) {
  unsigned char ipout[16];
  size_t iplen;

  iplen = (size_t)x509v3_a2i_ipadd(ipout, ipasc);
  if (iplen == 0) {
    return 0;
  }
  return X509_VERIFY_PARAM_set1_ip(param, ipout, iplen);
}

int X509_VERIFY_PARAM_get_depth(const X509_VERIFY_PARAM *param) {
  return param->depth;
}

const char *X509_VERIFY_PARAM_get0_name(const X509_VERIFY_PARAM *param) {
  return param->name;
}

#define vpm_empty_id NULL, 0U, NULL, NULL, 0, NULL, 0, 0

// Default verify parameters: these are used for various applications and can
// be overridden by the user specified table. NB: the 'name' field *must* be
// in alphabetical order because it will be searched using OBJ_search.

static const X509_VERIFY_PARAM default_table[] = {
    {(char *)"default",          // X509 default parameters
     0,                          // Check time
     0,                          // internal flags
     X509_V_FLAG_TRUSTED_FIRST,  // flags
     0,                          // purpose
     0,                          // trust
     100,                        // depth
     NULL,                       // policies
     vpm_empty_id},
    {(char *)"pkcs7",          // S/MIME sign parameters
     0,                        // Check time
     0,                        // internal flags
     0,                        // flags
     X509_PURPOSE_SMIME_SIGN,  // purpose
     X509_TRUST_EMAIL,         // trust
     -1,                       // depth
     NULL,                     // policies
     vpm_empty_id},
    {(char *)"smime_sign",     // S/MIME sign parameters
     0,                        // Check time
     0,                        // internal flags
     0,                        // flags
     X509_PURPOSE_SMIME_SIGN,  // purpose
     X509_TRUST_EMAIL,         // trust
     -1,                       // depth
     NULL,                     // policies
     vpm_empty_id},
    {(char *)"ssl_client",     // SSL/TLS client parameters
     0,                        // Check time
     0,                        // internal flags
     0,                        // flags
     X509_PURPOSE_SSL_CLIENT,  // purpose
     X509_TRUST_SSL_CLIENT,    // trust
     -1,                       // depth
     NULL,                     // policies
     vpm_empty_id},
    {(char *)"ssl_server",     // SSL/TLS server parameters
     0,                        // Check time
     0,                        // internal flags
     0,                        // flags
     X509_PURPOSE_SSL_SERVER,  // purpose
     X509_TRUST_SSL_SERVER,    // trust
     -1,                       // depth
     NULL,                     // policies
     vpm_empty_id}};

const X509_VERIFY_PARAM *X509_VERIFY_PARAM_lookup(const char *name) {
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(default_table); i++) {
    if (strcmp(default_table[i].name, name) == 0) {
      return &default_table[i];
    }
  }
  return NULL;
}
