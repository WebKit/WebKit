// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <string_view>

#include <openssl/asn1.h>
#include <openssl/base.h>
#include <openssl/base64.h>
#include <openssl/bio.h>
#include <openssl/buf.h>
#include <openssl/cipher.h>
#include <openssl/des.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include "../internal.h"
#include "internal.h"


#define MIN_LENGTH 4

using namespace bssl;

static int load_iv(const char **fromp, unsigned char *to, size_t num);
static int check_pem(const std::string_view nm, const std::string_view name);

// PEM_proc_type appends a Proc-Type header to |buf|, determined by |type|.
static void PEM_proc_type(char buf[PEM_BUFSIZE], int type) {
  const char *str;

  if (type == PEM_TYPE_ENCRYPTED) {
    str = "ENCRYPTED";
  } else if (type == PEM_TYPE_MIC_CLEAR) {
    str = "MIC-CLEAR";
  } else if (type == PEM_TYPE_MIC_ONLY) {
    str = "MIC-ONLY";
  } else {
    str = "BAD-TYPE";
  }

  OPENSSL_strlcat(buf, "Proc-Type: 4,", PEM_BUFSIZE);
  OPENSSL_strlcat(buf, str, PEM_BUFSIZE);
  OPENSSL_strlcat(buf, "\n", PEM_BUFSIZE);
}

// PEM_dek_info appends a DEK-Info header to |buf|, with an algorithm of |type|
// and a single parameter, specified by hex-encoding |len| bytes from |str|.
static void PEM_dek_info(char buf[PEM_BUFSIZE], const char *type, size_t len,
                         char *str) {
  static const unsigned char map[17] = "0123456789ABCDEF";

  OPENSSL_strlcat(buf, "DEK-Info: ", PEM_BUFSIZE);
  OPENSSL_strlcat(buf, type, PEM_BUFSIZE);
  OPENSSL_strlcat(buf, ",", PEM_BUFSIZE);

  const size_t used = strlen(buf);
  const size_t available = PEM_BUFSIZE - used;
  if (len * 2 < len || len * 2 + 2 < len || available < len * 2 + 2) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    buf[used + i * 2] = map[(str[i] >> 4) & 0x0f];
    buf[used + i * 2 + 1] = map[(str[i]) & 0x0f];
  }
  buf[used + len * 2] = '\n';
  buf[used + len * 2 + 1] = '\0';
}

void *PEM_ASN1_read(d2i_of_void *d2i, const char *name, FILE *fp, void **x,
                    pem_password_cb *cb, void *u) {
  BIO *b = BIO_new_fp(fp, BIO_NOCLOSE);
  if (b == nullptr) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_BUF_LIB);
    return nullptr;
  }
  void *ret = PEM_ASN1_read_bio(d2i, name, b, x, cb, u);
  BIO_free(b);
  return ret;
}

static int check_pem(const std::string_view nm, const std::string_view name) {
  // Normal matching nm and name
  if (nm == name) {
    return 1;
  }

  // Make PEM_STRING_EVP_PKEY match any private key

  if (name == PEM_STRING_EVP_PKEY) {
    return nm == PEM_STRING_PKCS8 || nm == PEM_STRING_PKCS8INF ||
           nm == PEM_STRING_RSA || nm == PEM_STRING_EC || nm == PEM_STRING_DSA;
  }

  // Permit older strings

  if (nm == PEM_STRING_X509_OLD && name == PEM_STRING_X509) {
    return 1;
  }

  if (nm == PEM_STRING_X509_REQ_OLD && name == PEM_STRING_X509_REQ) {
    return 1;
  }

  // Allow normal certs to be read as trusted certs
  if (nm == PEM_STRING_X509 && name == PEM_STRING_X509_TRUSTED) {
    return 1;
  }

  if (nm == PEM_STRING_X509_OLD && name == PEM_STRING_X509_TRUSTED) {
    return 1;
  }

  // Some CAs use PKCS#7 with CERTIFICATE headers
  if (nm == PEM_STRING_X509 && name == PEM_STRING_PKCS7) {
    return 1;
  }

  if (nm == PEM_STRING_PKCS7_SIGNED && name == PEM_STRING_PKCS7) {
    return 1;
  }

#ifndef OPENSSL_NO_CMS
  if (nm == PEM_STRING_X509 && name == PEM_STRING_CMS) {
    return 1;
  }
  // Allow CMS to be read from PKCS#7 headers
  if (nm == PEM_STRING_PKCS7 && name == PEM_STRING_CMS) {
    return 1;
  }
#endif

  return 0;
}

static const EVP_CIPHER *cipher_by_name(const std::string_view name) {
  // This is similar to the (deprecated) function |EVP_get_cipherbyname|. Note
  // the PEM code assumes that ciphers have at least 8 bytes of IV, at most 20
  // bytes of overhead and generally behave like CBC mode.
  if (name == SN_des_cbc) {
    return EVP_des_cbc();
  } else if (name == SN_des_ede3_cbc) {
    return EVP_des_ede3_cbc();
  } else if (name == SN_aes_128_cbc) {
    return EVP_aes_128_cbc();
  } else if (name == SN_aes_192_cbc) {
    return EVP_aes_192_cbc();
  } else if (name == SN_aes_256_cbc) {
    return EVP_aes_256_cbc();
  } else {
    return nullptr;
  }
}

int PEM_bytes_read_bio(unsigned char **pdata, long *plen, char **pnm,
                       const char *name, BIO *bp, pem_password_cb *cb,
                       void *u) {
  EVP_CIPHER_INFO cipher;
  UniquePtr<char> nm;
  UniquePtr<char> header;
  Array<uint8_t> data;
  size_t ulen;
  size_t unused = 0;

  for (;;) {
    if (!PEM_read_bio_inner(bp, &nm, &header, &data)) {
      if (ERR_equals(ERR_peek_error(), ERR_LIB_PEM, PEM_R_NO_START_LINE)) {
        ERR_add_error_data(2, "Expecting: ", name);
      }
      return 0;
    }
    if (data.size() > LONG_MAX) {
      OPENSSL_PUT_ERROR(PEM, ERR_R_OVERFLOW);
      return 0;
    }
    if (check_pem(nm.get(), name)) {
      break;
    }
  }
  if (!PEM_get_EVP_CIPHER_INFO(header.get(), &cipher)) {
    return 0;
  }
  ulen = data.size();
  if (!PEM_do_header(&cipher, data.data(), &ulen, cb, u)) {
    return 0;
  }

  // Release the buffer to the caller.
  // Note that |PEM_do_header| may have reduced the length after decrypting
  // in-place.
  // This will not overflow because |data.size()| was checked to fit in |long|
  // above.
  data.Release(pdata, &unused);
  *plen = static_cast<long>(ulen);

  if (pnm) {
    *pnm = nm.release();
  }

  return 1;
}

int PEM_ASN1_write(i2d_of_void *i2d, const char *name, FILE *fp, void *x,
                   const EVP_CIPHER *enc, const unsigned char *pass,
                   int pass_len, pem_password_cb *callback, void *u) {
  BIO *b = BIO_new_fp(fp, BIO_NOCLOSE);
  if (b == nullptr) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_BUF_LIB);
    return 0;
  }
  int ret =
      PEM_ASN1_write_bio(i2d, name, b, x, enc, pass, pass_len, callback, u);
  BIO_free(b);
  return ret;
}

int PEM_ASN1_write_bio(i2d_of_void *i2d, const char *name, BIO *bp, void *x,
                       const EVP_CIPHER *enc, const unsigned char *pass,
                       int pass_len, pem_password_cb *callback, void *u) {
  ScopedEVP_CIPHER_CTX ctx;
  int dsize = 0, ret = 0;
  size_t i, j, data_size;
  unsigned char *p, *data = nullptr;
  const char *objstr = nullptr;
  char buf[PEM_BUFSIZE];
  unsigned char key[EVP_MAX_KEY_LENGTH];
  unsigned char iv[EVP_MAX_IV_LENGTH];

  if (enc != nullptr) {
    objstr = OBJ_nid2sn(EVP_CIPHER_nid(enc));
    if (objstr == nullptr || cipher_by_name(objstr) == nullptr ||
        EVP_CIPHER_iv_length(enc) < 8) {
      OPENSSL_PUT_ERROR(PEM, PEM_R_UNSUPPORTED_CIPHER);
      goto err;
    }
  }

  if ((dsize = i2d(x, nullptr)) < 0) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_ASN1_LIB);
    dsize = 0;
    goto err;
  }
  // dzise + 8 bytes are needed
  // actually it needs the cipher block size extra...
  data_size = static_cast<size_t>(dsize) + 20;
  data = (unsigned char *)OPENSSL_malloc(data_size);
  if (data == nullptr) {
    goto err;
  }
  p = data;
  i = i2d(x, &p);

  if (enc != nullptr) {
    const unsigned iv_len = EVP_CIPHER_iv_length(enc);

    if (pass == nullptr) {
      if (!callback) {
        callback = PEM_def_callback;
      }
      pass_len = (*callback)(buf, PEM_BUFSIZE, 1, u);
      if (pass_len < 0) {
        OPENSSL_PUT_ERROR(PEM, PEM_R_READ_KEY);
        goto err;
      }
      pass = (const unsigned char *)buf;
    }
    assert(iv_len <= sizeof(iv));
    if (!RAND_bytes(iv, iv_len)) {  // Generate a salt
      goto err;
    }
    // The 'iv' is used as the iv and as a salt.  It is NOT taken from
    // the BytesToKey function
    if (!EVP_BytesToKey(enc, EVP_md5(), iv, pass, pass_len, 1, key, nullptr)) {
      goto err;
    }

    if (pass == (const unsigned char *)buf) {
      OPENSSL_cleanse(buf, PEM_BUFSIZE);
    }

    assert(strlen(objstr) + 23 + 2 * iv_len + 13 <= sizeof(buf));

    buf[0] = '\0';
    PEM_proc_type(buf, PEM_TYPE_ENCRYPTED);
    PEM_dek_info(buf, objstr, iv_len, (char *)iv);
    // k=strlen(buf);

    ret = 1;
    if (!EVP_EncryptInit_ex(ctx.get(), enc, nullptr, key, iv) ||
        !EVP_EncryptUpdate_ex(ctx.get(), data, &j, data_size, data, i) ||
        !EVP_EncryptFinal_ex2(ctx.get(), &(data[j]), &i, data_size - j)) {
      ret = 0;
    } else {
      i += j;
    }
    if (ret == 0) {
      goto err;
    }
  } else {
    ret = 1;
    buf[0] = '\0';
  }
  i = PEM_write_bio(bp, name, buf, data, i);
  if (i <= 0) {
    ret = 0;
  }
err:
  OPENSSL_cleanse(key, sizeof(key));
  OPENSSL_cleanse(iv, sizeof(iv));
  OPENSSL_cleanse(buf, PEM_BUFSIZE);
  OPENSSL_free(data);
  return ret;
}

int bssl::PEM_do_header(const EVP_CIPHER_INFO *cipher, unsigned char *data,
                        size_t *len, pem_password_cb *callback, void *u) {
  int pass_len;
  ScopedEVP_CIPHER_CTX ctx;
  unsigned char key[EVP_MAX_KEY_LENGTH];
  char buf[PEM_BUFSIZE];
  const size_t in_len = *len;

  if (cipher->cipher == nullptr) {
    return 1;
  }

  pass_len = 0;
  if (!callback) {
    callback = PEM_def_callback;
  }
  pass_len = callback(buf, PEM_BUFSIZE, 0, u);
  if (pass_len < 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_PASSWORD_READ);
    return 0;
  }

  if (!EVP_BytesToKey(cipher->cipher, EVP_md5(), cipher->iv,
                      (unsigned char *)buf, pass_len, 1, key, nullptr)) {
    return 0;
  }

  // Safety: we have checked |*len| before narrowing so that |EVP_DecryptUpdate|
  // can safely work with it.
  size_t out_len1 = 0;
  size_t out_len2 = 0;
  if (!EVP_DecryptInit_ex(ctx.get(), cipher->cipher, nullptr, key,
                          cipher->iv) ||
      !EVP_DecryptUpdate_ex(ctx.get(), data, &out_len1, in_len, data, in_len) ||
      !EVP_DecryptFinal_ex2(ctx.get(), data + out_len1, &out_len2,
                            in_len - out_len1)) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_DECRYPT);
    return 0;
  }
  *len = out_len1 + out_len2;
  return 1;
}

int bssl::PEM_get_EVP_CIPHER_INFO(const char *header, EVP_CIPHER_INFO *cipher) {
  cipher->cipher = nullptr;
  OPENSSL_memset(cipher->iv, 0, sizeof(cipher->iv));
  if ((header == nullptr) || (*header == '\0') || (*header == '\n')) {
    return 1;
  }
  if (strncmp(header, "Proc-Type: ", 11) != 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_NOT_PROC_TYPE);
    return 0;
  }
  header += 11;
  if (header[0] != '4' || header[1] != ',') {
    OPENSSL_PUT_ERROR(PEM, PEM_R_UNSUPPORTED_PROC_TYPE_VERSION);
    return 0;
  }
  header += 2;
  if (strncmp(header, "ENCRYPTED", 9) != 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_NOT_ENCRYPTED);
    return 0;
  }
  for (; (*header != '\n') && (*header != '\0'); header++) {
    ;
  }
  if (*header == '\0') {
    OPENSSL_PUT_ERROR(PEM, PEM_R_SHORT_HEADER);
    return 0;
  }
  header++;
  if (strncmp(header, "DEK-Info: ", 10) != 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_NOT_DEK_INFO);
    return 0;
  }
  header += 10;

  const char *p = header;
  for (;;) {
    char c = *header;
    if (!((c >= 'A' && c <= 'Z') || c == '-' || OPENSSL_isdigit(c))) {
      break;
    }
    header++;
  }
  cipher->cipher = cipher_by_name(std::string_view(p, header - p));
  header++;
  if (cipher->cipher == nullptr) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_UNSUPPORTED_ENCRYPTION);
    return 0;
  }
  // The IV parameter must be at least 8 bytes long to be used as the salt in
  // the KDF. (This should not happen given |cipher_by_name|.)
  if (EVP_CIPHER_iv_length(cipher->cipher) < 8) {
    assert(0);
    OPENSSL_PUT_ERROR(PEM, PEM_R_UNSUPPORTED_ENCRYPTION);
    return 0;
  }
  const char **header_pp = &header;
  if (!load_iv(header_pp, cipher->iv, EVP_CIPHER_iv_length(cipher->cipher))) {
    return 0;
  }

  return 1;
}

static int load_iv(const char **fromp, unsigned char *to, size_t num) {
  uint8_t v;
  const char *from;

  from = *fromp;
  for (size_t i = 0; i < num; i++) {
    to[i] = 0;
  }
  num *= 2;
  for (size_t i = 0; i < num; i++) {
    if (!OPENSSL_fromxdigit(&v, *from)) {
      OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_IV_CHARS);
      return 0;
    }
    from++;
    to[i / 2] |= v << (!(i & 1)) * 4;
  }

  *fromp = from;
  return 1;
}

int PEM_write(FILE *fp, const char *name, const char *header,
              const unsigned char *data, long len) {
  BIO *b = BIO_new_fp(fp, BIO_NOCLOSE);
  if (b == nullptr) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_BUF_LIB);
    return 0;
  }
  int ret = PEM_write_bio(b, name, header, data, len);
  BIO_free(b);
  return ret;
}

int PEM_write_bio(BIO *bp, const char *name, const char *header,
                  const unsigned char *data, long len) {
  int nlen, n, i, j, outl;
  unsigned char *buf = nullptr;
  EVP_ENCODE_CTX ctx;
  int reason = ERR_R_BUF_LIB;
  int retval = 0;

  EVP_EncodeInit(&ctx);
  nlen = strlen(name);

  if ((BIO_write(bp, "-----BEGIN ", 11) != 11) ||
      (BIO_write(bp, name, nlen) != nlen) ||
      (BIO_write(bp, "-----\n", 6) != 6)) {
    goto err;
  }

  i = strlen(header);
  if (i > 0) {
    if ((BIO_write(bp, header, i) != i) || (BIO_write(bp, "\n", 1) != 1)) {
      goto err;
    }
  }

  buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(PEM_BUFSIZE * 8));
  if (buf == nullptr) {
    goto err;
  }

  i = j = 0;
  while (len > 0) {
    n = (int)((len > (PEM_BUFSIZE * 5)) ? (PEM_BUFSIZE * 5) : len);
    EVP_EncodeUpdate(&ctx, buf, &outl, &(data[j]), n);
    if ((outl) && (BIO_write(bp, (char *)buf, outl) != outl)) {
      goto err;
    }
    i += outl;
    len -= n;
    j += n;
  }
  EVP_EncodeFinal(&ctx, buf, &outl);
  if ((outl > 0) && (BIO_write(bp, (char *)buf, outl) != outl)) {
    goto err;
  }
  if ((BIO_write(bp, "-----END ", 9) != 9) ||
      (BIO_write(bp, name, nlen) != nlen) ||
      (BIO_write(bp, "-----\n", 6) != 6)) {
    goto err;
  }
  retval = i + outl;

err:
  if (retval == 0) {
    OPENSSL_PUT_ERROR(PEM, reason);
  }
  OPENSSL_free(buf);
  return retval;
}

int PEM_read(FILE *fp, char **name, char **header, unsigned char **data,
             long *len) {
  BIO *b = BIO_new_fp(fp, BIO_NOCLOSE);
  if (b == nullptr) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_BUF_LIB);
    return 0;
  }
  int ret = PEM_read_bio(b, name, header, data, len);
  BIO_free(b);
  return ret;
}

int bssl::PEM_read_bio_inner(BIO *bp, UniquePtr<char> *name,
                             UniquePtr<char> *header, Array<uint8_t> *data) {
  bssl::UniquePtr<BUF_MEM> nameB(BUF_MEM_new());
  bssl::UniquePtr<BUF_MEM> headerB(BUF_MEM_new());
  bssl::UniquePtr<BUF_MEM> dataB(BUF_MEM_new());
  if ((nameB == nullptr) || (headerB == nullptr) || (dataB == nullptr)) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_MALLOC_FAILURE);
    return 0;
  }

  char buf[256];  // 254 characters + newline + \0.
  buf[254] = '\0';
  // Invariant: buf[254] < ' '. It may get overwritten by '\n' or '\0' only.
  for (;;) {
    int i = BIO_gets(bp, buf, 254);
    if (i <= 0) {
      OPENSSL_PUT_ERROR(PEM, PEM_R_NO_START_LINE);
      return 0;
    }

    while ((i >= 0) && (buf[i] <= ' ')) {
      i--;
    }
    buf[++i] = '\n';
    buf[++i] = '\0';

    if (strncmp(buf, "-----BEGIN ", 11) == 0) {
      i = strlen(&(buf[11]));

      if (strncmp(&(buf[11 + i - 6]), "-----\n", 6) != 0) {
        continue;
      }
      if (!BUF_MEM_grow(nameB.get(), i - 5)) {
        OPENSSL_PUT_ERROR(PEM, ERR_R_MALLOC_FAILURE);
        return 0;
      }
      OPENSSL_memcpy(nameB->data, &(buf[11]), i - 6);
      nameB->data[i - 6] = '\0';
      break;
    }
  }

  size_t hl = 0;
  if (!BUF_MEM_grow(headerB.get(), 256)) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  headerB->data[0] = '\0';

  size_t bl = 0;
  if (!BUF_MEM_grow(dataB.get(), 1024)) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  dataB->data[0] = '\0';

  bool failed = false;  // Set to true (and error put into queue) on failure.
  auto read_until_end = [&](BUF_MEM *out, size_t &out_len,
                            bool stop_at_newline) {
    // Invariant: buf[254] < ' '. It may get overwritten by '\n' or '\0' only.
    for (;;) {
      int i = BIO_gets(bp, buf, 254);
      if (i <= 0) {
        break;
      }
      while ((i >= 0) && (buf[i] <= ' ')) {
        i--;
      }
      buf[++i] = '\n';
      buf[++i] = '\0';

      if (stop_at_newline && buf[0] == '\n') {
        // End of header, start of body.
        return true;
      }
      if (strncmp(buf, "-----END ", 9) == 0) {
        return false;
      }
      if (static_cast<size_t>(i) > SIZE_MAX - hl - 1) {
        OPENSSL_PUT_ERROR(PEM, ERR_R_OVERFLOW);
        failed = true;
        return false;
      }
      size_t new_len = out_len + i;
      if (new_len > INT_MAX / 2) {
        // Arbitrarily limit PEM data to INT_MAX / 2 bytes, which "ought to be
        // enough for anyone". Hardens against possible integer overflows
        // downstream.
        OPENSSL_PUT_ERROR(PEM, ERR_R_OVERFLOW);
        failed = true;
        return false;
      }
      if (!BUF_MEM_grow(out, new_len + 1)) {
        OPENSSL_PUT_ERROR(PEM, ERR_R_MALLOC_FAILURE);
        failed = true;
        return false;
      }
      OPENSSL_memcpy(&(out->data[out_len]), buf, i);
      out->data[new_len] = '\0';
      out_len = new_len;
    }
    return false;
  };

  if (read_until_end(headerB.get(), hl, /*stop_at_newline=*/true)) {
    read_until_end(dataB.get(), bl, /*stop_at_newline=*/false);
  } else {
    // Actually we've read the body, as there is no header.
    std::swap(hl, bl);
    std::swap(headerB, dataB);
  }
  if (failed) {
    return 0;
  }

  size_t name_len = strlen(nameB->data);
  if ((strncmp(buf, "-----END ", 9) != 0) ||
      (strncmp(&(buf[9]), nameB->data, name_len) != 0) ||
      (strncmp(&(buf[9 + name_len]), "-----\n", 6) != 0)) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_END_LINE);
    return 0;
  }

  EVP_ENCODE_CTX ctx;
  EVP_DecodeInit(&ctx);
  int decoded_length;
  int status =
      EVP_DecodeUpdate(&ctx, (unsigned char *)dataB->data, &decoded_length,
                       (unsigned char *)dataB->data, bl);
  if (status < 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_BASE64_DECODE);
    return 0;
  }
  int k;
  status = EVP_DecodeFinal(&ctx, (unsigned char *)&(dataB->data[bl]), &k);
  if (status < 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_BAD_BASE64_DECODE);
    return 0;
  }
  if (k > INT_MAX - decoded_length) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_OVERFLOW);
    return 0;
  }
  decoded_length += k;

  if (decoded_length == 0) {
    OPENSSL_PUT_ERROR(PEM, PEM_R_NO_DATA);
    return 0;
  }

  // Transfer ownership of buffers
  name->reset(nameB->data);
  nameB->data = nullptr;
  header->reset(headerB->data);
  headerB->data = nullptr;
  data->Reset((uint8_t *)dataB->data, decoded_length);
  dataB->data = nullptr;

  return 1;
}

int PEM_read_bio(BIO *bp, char **name, char **header, unsigned char **data,
                 long *len) {
  UniquePtr<char> owned_name;
  UniquePtr<char> owned_header;
  Array<uint8_t> owned_data;
  if (!PEM_read_bio_inner(bp, &owned_name, &owned_header, &owned_data)) {
    return 0;
  }
  if (owned_data.size() > LONG_MAX) {
    OPENSSL_PUT_ERROR(PEM, ERR_R_OVERFLOW);
    return 0;
  }
  size_t ulen = 0;
  *name = owned_name.release();
  *header = owned_header.release();
  owned_data.Release(data, &ulen);
  // Safety: we checked that |ulen| <= |LONG_MAX|.
  *len = static_cast<long>(ulen);
  return 1;
}

int PEM_def_callback(char *buf, int size, int rwflag, void *userdata) {
  if (!buf || !userdata || size < 0) {
    return -1;
  }
  size_t len = strlen((char *)userdata);
  if (len >= (size_t)size) {
    return -1;
  }
  OPENSSL_strlcpy(buf, reinterpret_cast<char *>(userdata), (size_t)size);
  return (int)len;
}
