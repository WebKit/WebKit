/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2004 The OpenSSL Project.  All rights reserved.
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

#include <stdarg.h>
#include <string.h>

#include <gtest/gtest.h>

#include <openssl/crypto.h>
#include <openssl/mem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "../internal.h"
#include "internal.h"


static const char *const names[] = {
    "a", "b", ".", "*", "@",
    ".a", "a.", ".b", "b.", ".*", "*.", "*@", "@*", "a@", "@a", "b@", "..",
    "-example.com", "example-.com",
    "@@", "**", "*.com", "*com", "*.*.com", "*com", "com*", "*example.com",
    "*@example.com", "test@*.example.com", "example.com", "www.example.com",
    "test.www.example.com", "*.example.com", "*.www.example.com",
    "test.*.example.com", "www.*.com",
    ".www.example.com", "*www.example.com",
    "example.net", "xn--rger-koa.example.com",
    "*.xn--rger-koa.example.com", "www.xn--rger-koa.example.com",
    "*.good--example.com", "www.good--example.com",
    "*.xn--bar.com", "xn--foo.xn--bar.com",
    "a.example.com", "b.example.com",
    "postmaster@example.com", "Postmaster@example.com",
    "postmaster@EXAMPLE.COM",
    NULL
};

static const char *const exceptions[] = {
    "set CN: host: [*.example.com] matches [a.example.com]",
    "set CN: host: [*.example.com] matches [b.example.com]",
    "set CN: host: [*.example.com] matches [www.example.com]",
    "set CN: host: [*.example.com] matches [xn--rger-koa.example.com]",
    "set CN: host: [*.www.example.com] matches [test.www.example.com]",
    "set CN: host: [*.www.example.com] matches [.www.example.com]",
    "set CN: host: [*www.example.com] matches [www.example.com]",
    "set CN: host: [test.www.example.com] matches [.www.example.com]",
    "set CN: host: [*.xn--rger-koa.example.com] matches [www.xn--rger-koa.example.com]",
    "set CN: host: [*.xn--bar.com] matches [xn--foo.xn--bar.com]",
    "set CN: host: [*.good--example.com] matches [www.good--example.com]",
    "set CN: host-no-wildcards: [*.www.example.com] matches [.www.example.com]",
    "set CN: host-no-wildcards: [test.www.example.com] matches [.www.example.com]",
    "set emailAddress: email: [postmaster@example.com] does not match [Postmaster@example.com]",
    "set emailAddress: email: [postmaster@EXAMPLE.COM] does not match [Postmaster@example.com]",
    "set emailAddress: email: [Postmaster@example.com] does not match [postmaster@example.com]",
    "set emailAddress: email: [Postmaster@example.com] does not match [postmaster@EXAMPLE.COM]",
    "set dnsName: host: [*.example.com] matches [www.example.com]",
    "set dnsName: host: [*.example.com] matches [a.example.com]",
    "set dnsName: host: [*.example.com] matches [b.example.com]",
    "set dnsName: host: [*.example.com] matches [xn--rger-koa.example.com]",
    "set dnsName: host: [*.www.example.com] matches [test.www.example.com]",
    "set dnsName: host-no-wildcards: [*.www.example.com] matches [.www.example.com]",
    "set dnsName: host-no-wildcards: [test.www.example.com] matches [.www.example.com]",
    "set dnsName: host: [*.www.example.com] matches [.www.example.com]",
    "set dnsName: host: [*www.example.com] matches [www.example.com]",
    "set dnsName: host: [test.www.example.com] matches [.www.example.com]",
    "set dnsName: host: [*.xn--rger-koa.example.com] matches [www.xn--rger-koa.example.com]",
    "set dnsName: host: [*.xn--bar.com] matches [xn--foo.xn--bar.com]",
    "set dnsName: host: [*.good--example.com] matches [www.good--example.com]",
    "set rfc822Name: email: [postmaster@example.com] does not match [Postmaster@example.com]",
    "set rfc822Name: email: [Postmaster@example.com] does not match [postmaster@example.com]",
    "set rfc822Name: email: [Postmaster@example.com] does not match [postmaster@EXAMPLE.COM]",
    "set rfc822Name: email: [postmaster@EXAMPLE.COM] does not match [Postmaster@example.com]",
    NULL
};

static int is_exception(const char *msg)
{
    const char *const *p;
    for (p = exceptions; *p; ++p)
        if (strcmp(msg, *p) == 0)
            return 1;
    return 0;
}

static int set_cn(X509 *crt, ...)
{
    int ret = 0;
    X509_NAME *n = NULL;
    va_list ap;
    va_start(ap, crt);
    n = X509_NAME_new();
    if (n == NULL)
        goto out;
    while (1) {
        int nid;
        const char *name;
        nid = va_arg(ap, int);
        if (nid == 0)
            break;
        name = va_arg(ap, const char *);
        if (!X509_NAME_add_entry_by_NID(n, nid, MBSTRING_ASC,
                                        (unsigned char *)name, -1, -1, 1))
            goto out;
    }
    if (!X509_set_subject_name(crt, n))
        goto out;
    ret = 1;
 out:
    X509_NAME_free(n);
    va_end(ap);
    return ret;
}

/*
 * int X509_add_ext(X509 *x, X509_EXTENSION *ex, int loc); X509_EXTENSION
 * *X509_EXTENSION_create_by_NID(X509_EXTENSION **ex, int nid, int crit,
 * ASN1_OCTET_STRING *data); int X509_add_ext(X509 *x, X509_EXTENSION *ex,
 * int loc);
 */

static int set_altname(X509 *crt, ...)
{
    int ret = 0;
    GENERAL_NAMES *gens = NULL;
    GENERAL_NAME *gen = NULL;
    ASN1_IA5STRING *ia5 = NULL;
    va_list ap;
    va_start(ap, crt);
    gens = sk_GENERAL_NAME_new_null();
    if (gens == NULL)
        goto out;
    while (1) {
        int type;
        const char *name;
        type = va_arg(ap, int);
        if (type == 0)
            break;
        name = va_arg(ap, const char *);

        gen = GENERAL_NAME_new();
        if (gen == NULL)
            goto out;
        ia5 = ASN1_IA5STRING_new();
        if (ia5 == NULL)
            goto out;
        if (!ASN1_STRING_set(ia5, name, -1))
            goto out;
        switch (type) {
        case GEN_EMAIL:
        case GEN_DNS:
            GENERAL_NAME_set0_value(gen, type, ia5);
            ia5 = NULL;
            break;
        default:
            abort();
        }
        sk_GENERAL_NAME_push(gens, gen);
        gen = NULL;
    }
    if (!X509_add1_ext_i2d(crt, NID_subject_alt_name, gens, 0, 0))
        goto out;
    ret = 1;
 out:
    ASN1_IA5STRING_free(ia5);
    GENERAL_NAME_free(gen);
    GENERAL_NAMES_free(gens);
    va_end(ap);
    return ret;
}

static int set_cn1(X509 *crt, const char *name)
{
    return set_cn(crt, NID_commonName, name, 0);
}

static int set_cn_and_email(X509 *crt, const char *name)
{
    return set_cn(crt, NID_commonName, name,
                  NID_pkcs9_emailAddress, "dummy@example.com", 0);
}

static int set_cn2(X509 *crt, const char *name)
{
    return set_cn(crt, NID_commonName, "dummy value",
                  NID_commonName, name, 0);
}

static int set_cn3(X509 *crt, const char *name)
{
    return set_cn(crt, NID_commonName, name,
                  NID_commonName, "dummy value", 0);
}

static int set_email1(X509 *crt, const char *name)
{
    return set_cn(crt, NID_pkcs9_emailAddress, name, 0);
}

static int set_email2(X509 *crt, const char *name)
{
    return set_cn(crt, NID_pkcs9_emailAddress, "dummy@example.com",
                  NID_pkcs9_emailAddress, name, 0);
}

static int set_email3(X509 *crt, const char *name)
{
    return set_cn(crt, NID_pkcs9_emailAddress, name,
                  NID_pkcs9_emailAddress, "dummy@example.com", 0);
}

static int set_email_and_cn(X509 *crt, const char *name)
{
    return set_cn(crt, NID_pkcs9_emailAddress, name,
                  NID_commonName, "www.example.org", 0);
}

static int set_altname_dns(X509 *crt, const char *name)
{
    return set_altname(crt, GEN_DNS, name, 0);
}

static int set_altname_email(X509 *crt, const char *name)
{
    return set_altname(crt, GEN_EMAIL, name, 0);
}

struct set_name_fn {
    int (*fn) (X509 *, const char *);
    const char *name;
    int host;
    int email;
};

static const struct set_name_fn name_fns[] = {
    {set_cn1, "set CN", 1, 0},
    {set_cn2, "set CN", 1, 0},
    {set_cn3, "set CN", 1, 0},
    {set_cn_and_email, "set CN", 1, 0},
    {set_email1, "set emailAddress", 0, 1},
    {set_email2, "set emailAddress", 0, 1},
    {set_email3, "set emailAddress", 0, 1},
    {set_email_and_cn, "set emailAddress", 0, 1},
    {set_altname_dns, "set dnsName", 1, 0},
    {set_altname_email, "set rfc822Name", 0, 1},
    {NULL, NULL, 0, 0},
};

static X509 *make_cert(void)
{
    X509 *ret = NULL;
    X509 *crt = NULL;
    X509_NAME *issuer = NULL;
    crt = X509_new();
    if (crt == NULL)
        goto out;
    if (!X509_set_version(crt, X509_VERSION_3))
        goto out;
    ret = crt;
    crt = NULL;
 out:
    X509_NAME_free(issuer);
    return ret;
}

static int errors;

static void check_message(const struct set_name_fn *fn, const char *op,
                          const char *nameincert, int match, const char *name)
{
    char msg[1024];
    if (match < 0)
        return;
    BIO_snprintf(msg, sizeof(msg), "%s: %s: [%s] %s [%s]",
                 fn->name, op, nameincert,
                 match ? "matches" : "does not match", name);
    if (is_exception(msg))
        return;
    puts(msg);
    ++errors;
}

static void run_cert(X509 *crt, const char *nameincert,
                     const struct set_name_fn *fn)
{
    const char *const *pname = names;
    while (*pname) {
        int samename = OPENSSL_strcasecmp(nameincert, *pname) == 0;
        size_t namelen = strlen(*pname);
        char *name = (char *)malloc(namelen);
        int match, ret;
        OPENSSL_memcpy(name, *pname, namelen);

        ret = X509_check_host(crt, name, namelen, 0, NULL);
        match = -1;
        if (ret < 0) {
            fprintf(stderr, "internal error in X509_check_host\n");
            ++errors;
        } else if (fn->host) {
            if (ret == 1 && !samename)
                match = 1;
            if (ret == 0 && samename)
                match = 0;
        } else if (ret == 1)
            match = 1;
        check_message(fn, "host", nameincert, match, *pname);

        ret = X509_check_host(crt, name, namelen,
                              X509_CHECK_FLAG_NO_WILDCARDS, NULL);
        match = -1;
        if (ret < 0) {
            fprintf(stderr, "internal error in X509_check_host\n");
            ++errors;
        } else if (fn->host) {
            if (ret == 1 && !samename)
                match = 1;
            if (ret == 0 && samename)
                match = 0;
        } else if (ret == 1)
            match = 1;
        check_message(fn, "host-no-wildcards", nameincert, match, *pname);

        ret = X509_check_email(crt, name, namelen, 0);
        match = -1;
        if (fn->email) {
            if (ret && !samename)
                match = 1;
            if (!ret && samename && strchr(nameincert, '@') != NULL)
                match = 0;
        } else if (ret)
            match = 1;
        check_message(fn, "email", nameincert, match, *pname);
        ++pname;
        free(name);
    }
}

// TODO(davidben): Convert this test to GTest more thoroughly.
TEST(X509V3Test, NameTest) {
    const struct set_name_fn *pfn = name_fns;
    while (pfn->name) {
        const char *const *pname = names;
        while (*pname) {
            // The common name fallback requires the name look sufficiently
            // DNS-like.
            if (strcmp(pfn->name, "set CN") == 0 &&
                !x509v3_looks_like_dns_name(
                    reinterpret_cast<const unsigned char*>(*pname),
                    strlen(*pname))) {
                ++pname;
                continue;
            }
            bssl::UniquePtr<X509> crt(make_cert());
            ASSERT_TRUE(crt);
            ASSERT_TRUE(pfn->fn(crt.get(), *pname));
            run_cert(crt.get(), *pname, pfn);
            ++pname;
        }
        ++pfn;
    }
    EXPECT_EQ(0, errors);
}
