// Copyright 2003-2016 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string_view>

#include <stdio.h>
#include <string.h>

#include <utility>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/base.h>
#include <openssl/bio.h>
#include <openssl/bytestring.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/span.h>
#include <openssl/stack.h>
#include <openssl/x509.h>

#include "../internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN
namespace {

int starts_with(const CBS *cbs, uint8_t c) {
  return CBS_len(cbs) > 0 && CBS_data(cbs)[0] == c;
}

int starts_with_str(const CBS *cbs, std::string_view str) {
  return CBS_len(cbs) >= str.size() &&
         !OPENSSL_memcmp(CBS_data(cbs), str.data(), str.size());
}

int ends_with(const CBS *cbs, uint8_t c) {
  return CBS_len(cbs) > 0 && CBS_data(cbs)[CBS_len(cbs) - 1] == c;
}

int equal_case(const CBS *a, const CBS *b) {
  if (CBS_len(a) != CBS_len(b)) {
    return 0;
  }
  // Note we cannot use |OPENSSL_strncasecmp| because that would stop
  // iterating at NUL.
  const uint8_t *a_data = CBS_data(a), *b_data = CBS_data(b);
  for (size_t i = 0; i < CBS_len(a); i++) {
    if (OPENSSL_tolower(a_data[i]) != OPENSSL_tolower(b_data[i])) {
      return 0;
    }
  }
  return 1;
}

int has_suffix_case(const CBS *a, const CBS *b) {
  if (CBS_len(a) < CBS_len(b)) {
    return 0;
  }
  CBS copy = *a;
  CBS_skip(&copy, CBS_len(a) - CBS_len(b));
  return equal_case(&copy, b);
}

bool is_allowed_rfc822_local_part(const CBS *cbs) {
  if (CBS_len(cbs) == 0) {
    return false;
  }
  for (size_t i = 0; i < CBS_len(cbs); i++) {
    uint8_t c = CBS_data(cbs)[i];
    if (!(OPENSSL_isalnum(c) || c == '!' || c == '#' || c == '$' || c == '%' ||
          c == '&' || c == '\'' || c == '*' || c == '+' || c == '-' ||
          c == '/' || c == '=' || c == '?' || c == '^' || c == '_' ||
          c == '`' || c == '{' || c == '|' || c == '}' || c == '~' ||
          c == '.')) {
      return false;
    }
  }
  return true;
}

bool is_allowed_rfc822_domain(const CBS *cbs) {
  if (CBS_len(cbs) == 0) {
    return false;
  }
  for (size_t i = 0; i < CBS_len(cbs); i++) {
    uint8_t c = CBS_data(cbs)[i];
    if (!(OPENSSL_isalnum(c) || c == '-' || c == '.')) {
      return false;
    }
  }
  return true;
}

// Removes the port part of a URI authority string, if present, leaving the
// host. Returns true if the port was syntactically valid (contained only
// digits), or was not present (vacuously valid).
bool nc_uri_remove_port(CBS *in_out) {
  CBS host, unused;
  if (CBS_get_until_first(in_out, &host, ':')) {
    if (!CBS_skip(in_out, 1) ||
        CBS_get_until_first_not_of(in_out, &unused, "0123456789")) {
      return false;
    }
    *in_out = host;
    return true;
  }
  // There was no port.
  return true;
}

// Strips a single trailing dot from `host` if present.
// Per RFC 9499, we allow names to be written relative to the root (e.g.
// "www.example.com" is allowed even though technically the FQDN is
// "www.example.com." with a trailing dot representing the common root), and
// normalize names into this relative form for consistency.
void nc_uri_remove_trailing_dot(CBS *host) {
  if (ends_with(host, '.')) {
    uint8_t unused_byte;
    BSSL_CHECK(CBS_get_last_u8(host, &unused_byte));
  }
}

// Returns whether the authority portion of a URI contains a syntactically valid
// fully qualified domain name (FQDN).
//
// RFC 3986, section 3.2:
//   authority   = [ userinfo "@" ] host [ ":" port ]
//
// RFC 5280, section 4.2.1.10: if a URI name constraint applies, applications
// MUST reject a certificate with a subjectAltName with a URI that "does not
// include an authority component with a host name specified as a fully
// qualified domain name".
//
// Therefore we reject IP addresses in this function, and reject authority
// components containing a userinfo component. The caller is responsible for
// having removed any colon and port component, and normalizing to DNS relative
// form by removing any trailing dot.
bool nc_uri_is_fqdn(const CBS *uri_authority) {
  if (CBS_len(uri_authority) == 0) {
    return false;
  }
  CBS host_to_parse = *uri_authority;

  // Reject userinfo, and IPv6 addresses delimited by square brackets. IPv4
  // addresses are rejected later.
  CBS unused;
  if (CBS_get_until_first_of(&host_to_parse, &unused, "@[]")) {
    return false;
  }

  // Validate that the host is identified by a registered name. Following RFC
  // 3986, section 3.2.2: we refer to the preferred name syntax rules for DNS
  // registered names found in RFC 1034, section 3.5, modified by RFC 1123:
  //
  //   <domain> ::= <subdomain> | " "
  //   <subdomain> ::= <label> | <subdomain> "." <label>
  //   <label> ::= <let-dig> [ [ <ldh-str> ] <let-dig> ]
  //   <ldh-str> ::= <let-dig-hyp> | <let-dig-hyp> <ldh-str>
  //   <let-dig-hyp> ::= <let-dig> | "-"
  //   <let-dig> ::= <letter> | <digit>
  //   <letter> ::= any one of the 52 alphabetic characters A through Z in
  //                upper case and a through z in lower case
  //   <digit> ::= any one of the ten digits 0 through 9
  //
  // Additionally:
  //  - The first and last character of each label is a letter or digit.
  //  - Label length is no more than 63 characters.
  CBS label;
  CBS_init(&label, nullptr, 0);
  while (CBS_len(&host_to_parse) > 0) {
    if (CBS_get_until_first(&host_to_parse, &label, '.')) {
      // Consume the dot.
      CBS_skip(&host_to_parse, 1);
      // A trailing dot should have already been removed by the caller.
      if (CBS_len(&host_to_parse) == 0) {
        return false;
      }
    } else {
      // This is the last label.
      label = host_to_parse;
      CBS_skip(&host_to_parse, CBS_len(&label));
    }
    if (CBS_len(&label) == 0 || CBS_len(&label) > 63 ||
        !OPENSSL_isalnum(CBS_data(&label)[0]) ||
        !OPENSSL_isalnum(CBS_data(&label)[CBS_len(&label) - 1])) {
      return false;
    }
    for (uint8_t c : Span<const uint8_t>(label)) {
      if (!OPENSSL_isalnum(c) && c != '-') {
        return false;
      }
    }
  }

  // Reject IP addresses by looking for (following the WHATWG URL parser) a
  // completely numeric (decimal, octal, or hex) last component. An empty final
  // component (e.g. if the host itself was empty) is also rejected here.
  // https://url.spec.whatwg.org/#concept-host-parser
  // https://url.spec.whatwg.org/#ends-in-a-number-checker
  //
  // Check for 0x or 0X followed by zero or more hex digits.
  if (starts_with_str(&label, "0x") || starts_with_str(&label, "0X")) {
    for (uint8_t c : Span<const uint8_t>(label).subspan(2)) {
      if (!OPENSSL_isxdigit(c)) {
        return true;
      }
    }
    return false;
  }
  // Check for decimal or octal digits.
  for (uint8_t c : Span<const uint8_t>(label)) {
    if (!OPENSSL_isdigit(c)) {
      return true;
    }
  }
  return false;
}

// Performs basic parsing of a URI to extract the host portion for name
// constraints matching. This is not a full URI parser. This returns false if no
// valid URI host could be obtained, which means the certificate should be
// rejected.
bool nc_get_valid_uri_host(CBS *out, const ASN1_IA5STRING *uri_name) {
  CBS uri_cbs;
  CBS_init(&uri_cbs, uri_name->data, uri_name->length);

  // RFC 3986, section 3:
  // URI         = scheme ":" hier-part [ "?" query ] [ "#" fragment ]

  // Check for the scheme and skip past it.
  // RFC 3986, section 3.1:
  //   scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
  CBS scheme;
  uint8_t byte;
  if (!CBS_get_until_first(&uri_cbs, &scheme, ':') ||
      // Consume the colon.
      !CBS_get_u8(&uri_cbs, &byte) ||
      // Test that the first byte is a letter.
      !CBS_get_u8(&scheme, &byte) || !OPENSSL_isalpha(byte)) {
    return false;
  }
  // Check that the rest of the scheme consists of allowed characters.
  for (uint8_t c : Span<const uint8_t>(scheme)) {
    if (!OPENSSL_isalnum(c) && c != '+' && c != '-' && c != '.') {
      return false;
    }
  }

  // RFC 3986, section 3.2:
  //   The authority component is preceded by a double slash ("//") and is
  //   terminated by the next slash ("/"), question mark ("?"), or number
  //   sign ("#") character, or by the end of the URI.
  if (!CBS_get_u8(&uri_cbs, &byte) || byte != '/' ||
      !CBS_get_u8(&uri_cbs, &byte) || byte != '/') {
    return false;
  }
  CBS authority;
  if (!CBS_get_until_first_of(&uri_cbs, &authority, "/?#")) {
    authority = uri_cbs;
  }

  // RFC 5280, section 4.2.1.10: reject URI with no authority component.
  if (CBS_len(&authority) == 0) {
    return false;
  }

  if (!nc_uri_remove_port(&authority)) {
    return false;
  }

  CBS host = authority;
  nc_uri_remove_trailing_dot(&host);
  if (!nc_uri_is_fqdn(&host)) {
    return false;
  }
  *out = host;
  return true;
}

// directoryName name constraint matching. The canonical encoding of X509_NAME
// makes this comparison easy. It is matched if the constraint is a prefix of
// the name.
int nc_dn(const X509_NAME *nm, const X509_NAME *base) {
  const X509_NAME_CACHE *nm_cache = x509_name_get_cache(nm);
  if (nm_cache == nullptr) {
    return X509_V_ERR_OUT_OF_MEM;
  }
  const X509_NAME_CACHE *base_cache = x509_name_get_cache(base);
  if (base_cache == nullptr) {
    return X509_V_ERR_OUT_OF_MEM;
  }
  if (base_cache->canon.size() > nm_cache->canon.size()) {
    return X509_V_ERR_PERMITTED_VIOLATION;
  }
  if (base_cache->canon !=
      Span(nm_cache->canon).first(base_cache->canon.size())) {
    return X509_V_ERR_PERMITTED_VIOLATION;
  }
  return X509_V_OK;
}

// DNS name constraint matching.
int nc_dns(const ASN1_IA5STRING *dns, const ASN1_IA5STRING *base,
           bool excluding) {
  CBS dns_cbs, base_cbs;
  CBS_init(&dns_cbs, dns->data, dns->length);
  CBS_init(&base_cbs, base->data, base->length);

  // Empty matches everything
  if (CBS_len(&base_cbs) == 0) {
    return X509_V_OK;
  }

  // Normalize absolute DNS names by removing the trailing dot, if any.
  if (ends_with(&dns_cbs, '.')) {
    uint8_t unused;
    CBS_get_last_u8(&dns_cbs, &unused);
  }
  if (ends_with(&base_cbs, '.')) {
    uint8_t unused;
    CBS_get_last_u8(&base_cbs, &unused);
  }

  // Wildcard partial-match handling ("*.bar.com" matching name constraint
  // "foo.bar.com"). This only handles the case where the the dnsname and the
  // constraint match after removing the leftmost label, otherwise it is handled
  // by falling through to the check of whether the dnsname is fully within or
  // fully outside of the constraint.
  if (excluding && starts_with_str(&dns_cbs, "*.")) {
    CBS unused;
    CBS base_parent_cbs = base_cbs;
    CBS dns_parent_cbs = dns_cbs;
    CBS_skip(&dns_parent_cbs, 2);
    if (CBS_get_until_first(&base_parent_cbs, &unused, '.') &&
        CBS_skip(&base_parent_cbs, 1)) {
      if (equal_case(&dns_parent_cbs, &base_parent_cbs)) {
        return X509_V_OK;
      }
    }
  }

  // If |base_cbs| begins with a '.', do a simple suffix comparison. This is
  // not part of RFC5280, but is part of OpenSSL's original behavior.
  if (starts_with(&base_cbs, '.')) {
    if (has_suffix_case(&dns_cbs, &base_cbs)) {
      return X509_V_OK;
    }
    return X509_V_ERR_PERMITTED_VIOLATION;
  }

  // Otherwise can add zero or more components on the left so compare RHS
  // and if dns is longer and expect '.' as preceding character.
  if (CBS_len(&dns_cbs) > CBS_len(&base_cbs)) {
    uint8_t dot;
    if (!CBS_skip(&dns_cbs, CBS_len(&dns_cbs) - CBS_len(&base_cbs) - 1) ||
        !CBS_get_u8(&dns_cbs, &dot) || dot != '.') {
      return X509_V_ERR_PERMITTED_VIOLATION;
    }
  }

  if (!equal_case(&dns_cbs, &base_cbs)) {
    return X509_V_ERR_PERMITTED_VIOLATION;
  }

  return X509_V_OK;
}

int nc_email(const ASN1_IA5STRING *eml, const ASN1_IA5STRING *base,
             bool case_insensitive_local_part) {
  CBS eml_cbs, base_cbs;
  CBS_init(&eml_cbs, eml->data, eml->length);
  CBS_init(&base_cbs, base->data, base->length);

  CBS eml_local;
  if (!CBS_get_until_first(&eml_cbs, &eml_local, '@') ||
      !CBS_skip(&eml_cbs, 1)) {
    return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
  }
  CBS eml_domain = eml_cbs;

  // Exactly one '@' in eml.
  CBS unused;
  if (CBS_get_until_first(&eml_cbs, &unused, '@')) {
    return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
  }

  if (!is_allowed_rfc822_local_part(&eml_local) ||
      !is_allowed_rfc822_domain(&eml_domain)) {
    return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
  }

  CBS base_local, base_domain;
  bool base_has_at = CBS_get_until_first(&base_cbs, &base_local, '@');
  if (base_has_at) {
    CBS_skip(&base_cbs, 1);
    base_domain = base_cbs;
    if (CBS_get_until_first(&base_cbs, &unused, '@')) {
      // If we did the full parsing then it is possible for a @ to be in a
      // quoted local-part of the name, but we don't do that, so just error if @
      // appears more than once.
      return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
    }
    if (!is_allowed_rfc822_local_part(&base_local)) {
      return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
    }
  } else {
    base_domain = base_cbs;
  }

  if (!is_allowed_rfc822_domain(&base_domain)) {
    return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
  }

  // RFC 5280 section 4.2.1.10:
  // To indicate a particular mailbox, the constraint is the complete mail
  // address.  For example, "root@example.com" indicates the root mailbox on
  // the host "example.com".
  if (base_has_at) {
    bool local_part_matches =
        case_insensitive_local_part
            ? equal_case(&base_local, &eml_local)
            : CBS_mem_equal(&base_local, CBS_data(&eml_local),
                            CBS_len(&eml_local));
    bool domain_matches = equal_case(&base_domain, &eml_domain);
    if (!local_part_matches || !domain_matches) {
      return X509_V_ERR_PERMITTED_VIOLATION;
    }
    return X509_V_OK;
  }

  // RFC 5280 section 4.2.1.10:
  // To specify any address within a domain, the constraint is specified with a
  // leading period (as with URIs).  For example, ".example.com" indicates all
  // the Internet mail addresses in the domain "example.com", but not Internet
  // mail addresses on the host "example.com".
  if (starts_with(&base_domain, '.')) {
    if (has_suffix_case(&eml_domain, &base_domain)) {
      return X509_V_OK;
    }
    return X509_V_ERR_PERMITTED_VIOLATION;
  }

  // RFC 5280 section 4.2.1.10:
  // To indicate all Internet mail addresses on a particular host, the
  // constraint is specified as the host name.  For example, the constraint
  // "example.com" is satisfied by any mail address at the host "example.com".
  if (equal_case(&base_domain, &eml_domain)) {
    return X509_V_OK;
  }
  return X509_V_ERR_PERMITTED_VIOLATION;
}

int nc_uri(const ASN1_IA5STRING *uri, const ASN1_IA5STRING *base) {
  // Parse a URI name to extract the host name as a fully qualified domain name.
  CBS uri_host;
  if (!nc_get_valid_uri_host(&uri_host, uri)) {
    return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
  }

  CBS base_cbs;
  CBS_init(&base_cbs, base->data, base->length);
  nc_uri_remove_trailing_dot(&base_cbs);

  // Make a copy of the name constraint string, which will be modified and
  // checked for FQDN validity.
  CBS base_fqdn = base_cbs;

  // If present, strip a leading dot, which denotes a name constraint that
  // matches all DNS subdomains.
  const bool base_has_leading_dot = starts_with(&base_fqdn, '.');
  if (base_has_leading_dot) {
    CBS_skip(&base_fqdn, 1);
  }

  // Validate that the name constraint is a proper FQDN.
  if (!nc_uri_is_fqdn(&base_fqdn)) {
    return X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX;
  }

  // Special case: initial '.' is RHS match
  if (base_has_leading_dot) {
    if (has_suffix_case(&uri_host, &base_cbs)) {
      return X509_V_OK;
    }
    return X509_V_ERR_PERMITTED_VIOLATION;
  }

  if (!equal_case(&base_cbs, &uri_host)) {
    return X509_V_ERR_PERMITTED_VIOLATION;
  }

  return X509_V_OK;
}

int nc_match_single(const GENERAL_NAME *gen, const GENERAL_NAME *base,
                    bool excluding, bool case_insensitive_local_part) {
  switch (base->type) {
    case GEN_DIRNAME:
      return nc_dn(gen->d.directoryName, base->d.directoryName);

    case GEN_DNS:
      return nc_dns(gen->d.dNSName, base->d.dNSName, excluding);

    case GEN_EMAIL:
      return nc_email(gen->d.rfc822Name, base->d.rfc822Name,
                      case_insensitive_local_part);

    case GEN_URI:
      return nc_uri(gen->d.uniformResourceIdentifier,
                    base->d.uniformResourceIdentifier);

    default:
      return X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE;
  }
}

int nc_match(const GENERAL_NAME *gen, const NAME_CONSTRAINTS *nc,
             bool case_insensitive_exclude_localpart) {
  // Permitted subtrees: if any subtrees exist of the name type, then at least
  // one subtree must match. If no subtrees match the type, then that name type
  // is unconstrained.
  enum {
    kUnconstrainedNameType = 0,
    kNoMatch,
    kMatch,
  } match_status = kUnconstrainedNameType;
  for (const GENERAL_SUBTREE *sub : nc->permittedSubtrees) {
    if (gen->type != sub->base->type) {
      continue;
    }
    if (sub->minimum || sub->maximum) {
      return X509_V_ERR_SUBTREE_MINMAX;
    }
    // If we already have a match don't bother trying any more
    if (match_status == kMatch) {
      continue;
    }
    match_status = kNoMatch;
    int r = nc_match_single(gen, sub->base, /*excluding=*/false,
                            /*case_insensitive_local_part=*/false);
    if (r == X509_V_OK) {
      match_status = kMatch;
    } else if (r != X509_V_ERR_PERMITTED_VIOLATION) {
      return r;
    }
  }

  if (match_status == kNoMatch) {
    return X509_V_ERR_PERMITTED_VIOLATION;
  }

  // Excluded subtrees: must not match any of these
  for (const GENERAL_SUBTREE *sub : nc->excludedSubtrees) {
    if (gen->type != sub->base->type) {
      continue;
    }
    if (sub->minimum || sub->maximum) {
      return X509_V_ERR_SUBTREE_MINMAX;
    }

    int r = nc_match_single(gen, sub->base, /*excluding=*/true,
                            /*case_insensitive_local_part=*/
                            case_insensitive_exclude_localpart);
    if (r == X509_V_OK) {
      return X509_V_ERR_EXCLUDED_VIOLATION;
    } else if (r != X509_V_ERR_PERMITTED_VIOLATION) {
      return r;
    }
  }

  return X509_V_OK;
}

int print_nc_ipadd(BIO *bp, const ASN1_OCTET_STRING *ip) {
  int i, len;
  unsigned char *p;
  p = ip->data;
  len = ip->length;
  BIO_puts(bp, "IP:");
  if (len == 8) {
    BIO_printf(bp, "%d.%d.%d.%d/%d.%d.%d.%d", p[0], p[1], p[2], p[3], p[4],
               p[5], p[6], p[7]);
  } else if (len == 32) {
    for (i = 0; i < 16; i++) {
      uint16_t v = ((uint16_t)p[0] << 8) | p[1];
      BIO_printf(bp, "%X", v);
      p += 2;
      if (i == 7) {
        BIO_puts(bp, "/");
      } else if (i != 15) {
        BIO_puts(bp, ":");
      }
    }
  } else {
    BIO_printf(bp, "IP Address:<invalid>");
  }
  return 1;
}

int do_i2r_name_constraints(const X509V3_EXT_METHOD *method,
                            const STACK_OF(GENERAL_SUBTREE) *trees, BIO *bp,
                            int ind, const char *name) {
  if (sk_GENERAL_SUBTREE_num(trees) > 0) {
    BIO_printf(bp, "%*s%s:\n", ind, "", name);
  }
  for (const GENERAL_SUBTREE *tree : trees) {
    BIO_printf(bp, "%*s", ind + 2, "");
    if (tree->base->type == GEN_IPADD) {
      print_nc_ipadd(bp, tree->base->d.ip);
    } else {
      GENERAL_NAME_print(bp, tree->base);
    }
    BIO_puts(bp, "\n");
  }
  return 1;
}

int i2r_NAME_CONSTRAINTS(const X509V3_EXT_METHOD *method, void *a, BIO *bp,
                         int ind) {
  NAME_CONSTRAINTS *ncons = reinterpret_cast<NAME_CONSTRAINTS *>(a);
  do_i2r_name_constraints(method, ncons->permittedSubtrees, bp, ind,
                          "Permitted");
  do_i2r_name_constraints(method, ncons->excludedSubtrees, bp, ind, "Excluded");
  return 1;
}

void *v2i_NAME_CONSTRAINTS(const X509V3_EXT_METHOD *method,
                           const X509V3_CTX *ctx,
                           const STACK_OF(CONF_VALUE) *nval) {
  UniquePtr<NAME_CONSTRAINTS> ncons(NAME_CONSTRAINTS_new());
  if (!ncons) {
    return nullptr;
  }
  for (const CONF_VALUE *val : nval) {
    CONF_VALUE tval;
    STACK_OF(GENERAL_SUBTREE) **ptree = nullptr;
    if (!strncmp(val->name, "permitted", 9) && val->name[9]) {
      ptree = &ncons->permittedSubtrees;
      tval.name = val->name + 10;
    } else if (!strncmp(val->name, "excluded", 8) && val->name[8]) {
      ptree = &ncons->excludedSubtrees;
      tval.name = val->name + 9;
    } else {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_SYNTAX);
      return nullptr;
    }
    tval.value = val->value;
    UniquePtr<GENERAL_SUBTREE> sub(GENERAL_SUBTREE_new());
    if (sub == nullptr ||
        !v2i_GENERAL_NAME_ex(sub->base, method, ctx, &tval, 1)) {
      return nullptr;
    }
    if (!*ptree) {
      *ptree = sk_GENERAL_SUBTREE_new_null();
    }
    if (!*ptree || !PushToStack(*ptree, std::move(sub))) {
      return nullptr;
    }
  }

  return ncons.release();
}

}  // namespace
BSSL_NAMESPACE_END

using namespace bssl;

const X509V3_EXT_METHOD bssl::v3_name_constraints = {
    NID_name_constraints,
    0,
    ASN1_ITEM_ref(NAME_CONSTRAINTS),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    v2i_NAME_CONSTRAINTS,
    i2r_NAME_CONSTRAINTS,
    nullptr,
    nullptr,
};

ASN1_SEQUENCE(GENERAL_SUBTREE) = {
    ASN1_SIMPLE(GENERAL_SUBTREE, base, GENERAL_NAME),
    ASN1_IMP_OPT(GENERAL_SUBTREE, minimum, ASN1_INTEGER, 0),
    ASN1_IMP_OPT(GENERAL_SUBTREE, maximum, ASN1_INTEGER, 1),
} ASN1_SEQUENCE_END(GENERAL_SUBTREE)

ASN1_SEQUENCE(NAME_CONSTRAINTS) = {
    ASN1_IMP_SEQUENCE_OF_OPT(NAME_CONSTRAINTS, permittedSubtrees,
                             GENERAL_SUBTREE, 0),
    ASN1_IMP_SEQUENCE_OF_OPT(NAME_CONSTRAINTS, excludedSubtrees,
                             GENERAL_SUBTREE, 1),
} ASN1_SEQUENCE_END(NAME_CONSTRAINTS)

IMPLEMENT_ASN1_ALLOC_FUNCTIONS(GENERAL_SUBTREE)
IMPLEMENT_ASN1_ALLOC_FUNCTIONS(NAME_CONSTRAINTS)

//-
// Check a certificate conforms to a specified set of constraints.
// Return values:
//   X509_V_OK: All constraints obeyed.
//   X509_V_ERR_PERMITTED_VIOLATION: Permitted subtree violation.
//   X509_V_ERR_EXCLUDED_VIOLATION: Excluded subtree violation.
//   X509_V_ERR_SUBTREE_MINMAX: Min or max values present and matching type.
//   X509_V_ERR_UNSPECIFIED: Unspecified error.
//   X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE: Unsupported constraint type.
//   X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX: Bad or unsupported constraint
//     syntax.
//   X509_V_ERR_UNSUPPORTED_NAME_SYNTAX: Bad or unsupported syntax of name.
int NAME_CONSTRAINTS_check(const X509 *x, const NAME_CONSTRAINTS *nc) {
  const X509_NAME *nm = X509_get_subject_name(x);

  // Guard against certificates with an excessive number of names or
  // constraints causing a computationally expensive name constraints
  // check.
  const auto *x_impl = FromOpaque(x);
  size_t name_count =
      X509_NAME_entry_count(nm) + sk_GENERAL_NAME_num(x_impl->altname);
  size_t constraint_count = sk_GENERAL_SUBTREE_num(nc->permittedSubtrees) +
                            sk_GENERAL_SUBTREE_num(nc->excludedSubtrees);
  size_t check_count = constraint_count * name_count;
  if (name_count < (size_t)X509_NAME_entry_count(nm) ||
      constraint_count < sk_GENERAL_SUBTREE_num(nc->permittedSubtrees) ||
      (constraint_count && check_count / constraint_count != name_count) ||
      check_count > 1 << 20) {
    return X509_V_ERR_UNSPECIFIED;
  }

  if (X509_NAME_entry_count(nm) > 0) {
    GENERAL_NAME gntmp;
    gntmp.type = GEN_DIRNAME;
    gntmp.d.directoryName = const_cast<X509_NAME *>(nm);

    int r = nc_match(&gntmp, nc, /*case_insensitive_exclude_localpart=*/false);
    if (r != X509_V_OK) {
      return r;
    }

    gntmp.type = GEN_EMAIL;

    // Process any email address attributes in subject name
    for (int i = -1;;) {
      i = X509_NAME_get_index_by_NID(nm, NID_pkcs9_emailAddress, i);
      if (i == -1) {
        break;
      }
      const X509_NAME_ENTRY *ne = X509_NAME_get_entry(nm, i);
      gntmp.d.rfc822Name = X509_NAME_ENTRY_get_data(ne);
      if (gntmp.d.rfc822Name->type != V_ASN1_IA5STRING) {
        return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
      }

      // Whether local_part should be matched case-sensitive or not is somewhat
      // unclear. RFC 2821 says that it should be case-sensitive. RFC 2985 says
      // that emailAddress attributes in a Name are fully case-insensitive.
      // Some other verifier implementations always do local-part comparison
      // case-sensitive, while some always do it case-insensitive. Many but not
      // all SMTP servers interpret addresses as case-insensitive.
      //
      // Give how poorly specified this is, and the conflicting implementations
      // in the wild, this implementation will do case-insensitive match for
      // excluded names from the subject to avoid potentially allowing
      // something that wasn't expected.
      r = nc_match(&gntmp, nc, /*case_insensitive_exclude_localpart=*/true);
      if (r != X509_V_OK) {
        return r;
      }
    }
  }

  for (const GENERAL_NAME *gen : x_impl->altname) {
    int r = nc_match(gen, nc, /*case_insensitive_exclude_localpart=*/false);
    if (r != X509_V_OK) {
      return r;
    }
  }

  return X509_V_OK;
}
