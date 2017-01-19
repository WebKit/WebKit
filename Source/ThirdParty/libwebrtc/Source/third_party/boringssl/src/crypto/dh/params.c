/* ====================================================================
 * Copyright (c) 2011 The OpenSSL Project.  All rights reserved.
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

#include <openssl/dh.h>

#include <openssl/bn.h>

#include "../bn/internal.h"


static const BN_ULONG dh1024_160_p[] = {
    TOBN(0xDF1FB2BC, 0x2E4A4371), TOBN(0xE68CFDA7, 0x6D4DA708),
    TOBN(0x45BF37DF, 0x365C1A65), TOBN(0xA151AF5F, 0x0DC8B4BD),
    TOBN(0xFAA31A4F, 0xF55BCCC0), TOBN(0x4EFFD6FA, 0xE5644738),
    TOBN(0x98488E9C, 0x219A7372), TOBN(0xACCBDD7D, 0x90C4BD70),
    TOBN(0x24975C3C, 0xD49B83BF), TOBN(0x13ECB4AE, 0xA9061123),
    TOBN(0x9838EF1E, 0x2EE652C0), TOBN(0x6073E286, 0x75A23D18),
    TOBN(0x9A6A9DCA, 0x52D23B61), TOBN(0x52C99FBC, 0xFB06A3C6),
    TOBN(0xDE92DE5E, 0xAE5D54EC), TOBN(0xB10B8F96, 0xA080E01D),
};
static const BN_ULONG dh1024_160_g[] = {
    TOBN(0x855E6EEB, 0x22B3B2E5), TOBN(0x858F4DCE, 0xF97C2A24),
    TOBN(0x2D779D59, 0x18D08BC8), TOBN(0xD662A4D1, 0x8E73AFA3),
    TOBN(0x1DBF0A01, 0x69B6A28A), TOBN(0xA6A24C08, 0x7A091F53),
    TOBN(0x909D0D22, 0x63F80A76), TOBN(0xD7FBD7D3, 0xB9A92EE1),
    TOBN(0x5E91547F, 0x9E2749F4), TOBN(0x160217B4, 0xB01B886A),
    TOBN(0x777E690F, 0x5504F213), TOBN(0x266FEA1E, 0x5C41564B),
    TOBN(0xD6406CFF, 0x14266D31), TOBN(0xF8104DD2, 0x58AC507F),
    TOBN(0x6765A442, 0xEFB99905), TOBN(0xA4D1CBD5, 0xC3FD3412),
};
static const BN_ULONG dh1024_160_q[] = {
    TOBN(0x64B7CB9D, 0x49462353), TOBN(0x81A8DF27, 0x8ABA4E7D), 0xF518AA87,
};

static const BN_ULONG dh2048_224_p[] = {
    TOBN(0x0AC4DFFE, 0x0C10E64F), TOBN(0xCF9DE538, 0x4E71B81C),
    TOBN(0x7EF363E2, 0xFFA31F71), TOBN(0xE3FB73C1, 0x6B8E75B9),
    TOBN(0xC9B53DCF, 0x4BA80A29), TOBN(0x23F10B0E, 0x16E79763),
    TOBN(0xC52172E4, 0x13042E9B), TOBN(0xBE60E69C, 0xC928B2B9),
    TOBN(0x80CD86A1, 0xB9E587E8), TOBN(0x315D75E1, 0x98C641A4),
    TOBN(0xCDF93ACC, 0x44328387), TOBN(0x15987D9A, 0xDC0A486D),
    TOBN(0x7310F712, 0x1FD5A074), TOBN(0x278273C7, 0xDE31EFDC),
    TOBN(0x1602E714, 0x415D9330), TOBN(0x81286130, 0xBC8985DB),
    TOBN(0xB3BF8A31, 0x70918836), TOBN(0x6A00E0A0, 0xB9C49708),
    TOBN(0xC6BA0B2C, 0x8BBC27BE), TOBN(0xC9F98D11, 0xED34DBF6),
    TOBN(0x7AD5B7D0, 0xB6C12207), TOBN(0xD91E8FEF, 0x55B7394B),
    TOBN(0x9037C9ED, 0xEFDA4DF8), TOBN(0x6D3F8152, 0xAD6AC212),
    TOBN(0x1DE6B85A, 0x1274A0A6), TOBN(0xEB3D688A, 0x309C180E),
    TOBN(0xAF9A3C40, 0x7BA1DF15), TOBN(0xE6FA141D, 0xF95A56DB),
    TOBN(0xB54B1597, 0xB61D0A75), TOBN(0xA20D64E5, 0x683B9FD1),
    TOBN(0xD660FAA7, 0x9559C51F), TOBN(0xAD107E1E, 0x9123A9D0),
};

static const BN_ULONG dh2048_224_g[] = {
    TOBN(0x84B890D3, 0x191F2BFA), TOBN(0x81BC087F, 0x2A7065B3),
    TOBN(0x19C418E1, 0xF6EC0179), TOBN(0x7B5A0F1C, 0x71CFFF4C),
    TOBN(0xEDFE72FE, 0x9B6AA4BD), TOBN(0x81E1BCFE, 0x94B30269),
    TOBN(0x566AFBB4, 0x8D6C0191), TOBN(0xB539CCE3, 0x409D13CD),
    TOBN(0x6AA21E7F, 0x5F2FF381), TOBN(0xD9E263E4, 0x770589EF),
    TOBN(0x10E183ED, 0xD19963DD), TOBN(0xB70A8137, 0x150B8EEB),
    TOBN(0x051AE3D4, 0x28C8F8AC), TOBN(0xBB77A86F, 0x0C1AB15B),
    TOBN(0x6E3025E3, 0x16A330EF), TOBN(0x19529A45, 0xD6F83456),
    TOBN(0xF180EB34, 0x118E98D1), TOBN(0xB5F6C6B2, 0x50717CBE),
    TOBN(0x09939D54, 0xDA7460CD), TOBN(0xE2471504, 0x22EA1ED4),
    TOBN(0xB8A762D0, 0x521BC98A), TOBN(0xF4D02727, 0x5AC1348B),
    TOBN(0xC1766910, 0x1999024A), TOBN(0xBE5E9001, 0xA8D66AD7),
    TOBN(0xC57DB17C, 0x620A8652), TOBN(0xAB739D77, 0x00C29F52),
    TOBN(0xDD921F01, 0xA70C4AFA), TOBN(0xA6824A4E, 0x10B9A6F0),
    TOBN(0x74866A08, 0xCFE4FFE3), TOBN(0x6CDEBE7B, 0x89998CAF),
    TOBN(0x9DF30B5C, 0x8FFDAC50), TOBN(0xAC4032EF, 0x4F2D9AE3),
};

static const BN_ULONG dh2048_224_q[] = {
    TOBN(0xBF389A99, 0xB36371EB), TOBN(0x1F80535A, 0x4738CEBC),
    TOBN(0xC58D93FE, 0x99717710), 0x801C0D34,
};

static const BN_ULONG dh2048_256_p[] = {
    TOBN(0xDB094AE9, 0x1E1A1597), TOBN(0x693877FA, 0xD7EF09CA),
    TOBN(0x6116D227, 0x6E11715F), TOBN(0xA4B54330, 0xC198AF12),
    TOBN(0x75F26375, 0xD7014103), TOBN(0xC3A3960A, 0x54E710C3),
    TOBN(0xDED4010A, 0xBD0BE621), TOBN(0xC0B857F6, 0x89962856),
    TOBN(0xB3CA3F79, 0x71506026), TOBN(0x1CCACB83, 0xE6B486F6),
    TOBN(0x67E144E5, 0x14056425), TOBN(0xF6A167B5, 0xA41825D9),
    TOBN(0x3AD83477, 0x96524D8E), TOBN(0xF13C6D9A, 0x51BFA4AB),
    TOBN(0x2D525267, 0x35488A0E), TOBN(0xB63ACAE1, 0xCAA6B790),
    TOBN(0x4FDB70C5, 0x81B23F76), TOBN(0xBC39A0BF, 0x12307F5C),
    TOBN(0xB941F54E, 0xB1E59BB8), TOBN(0x6C5BFC11, 0xD45F9088),
    TOBN(0x22E0B1EF, 0x4275BF7B), TOBN(0x91F9E672, 0x5B4758C0),
    TOBN(0x5A8A9D30, 0x6BCF67ED), TOBN(0x209E0C64, 0x97517ABD),
    TOBN(0x3BF4296D, 0x830E9A7C), TOBN(0x16C3D911, 0x34096FAA),
    TOBN(0xFAF7DF45, 0x61B2AA30), TOBN(0xE00DF8F1, 0xD61957D4),
    TOBN(0x5D2CEED4, 0x435E3B00), TOBN(0x8CEEF608, 0x660DD0F2),
    TOBN(0xFFBBD19C, 0x65195999), TOBN(0x87A8E61D, 0xB4B6663C),
};
static const BN_ULONG dh2048_256_g[] = {
    TOBN(0x664B4C0F, 0x6CC41659), TOBN(0x5E2327CF, 0xEF98C582),
    TOBN(0xD647D148, 0xD4795451), TOBN(0x2F630784, 0x90F00EF8),
    TOBN(0x184B523D, 0x1DB246C3), TOBN(0xC7891428, 0xCDC67EB6),
    TOBN(0x7FD02837, 0x0DF92B52), TOBN(0xB3353BBB, 0x64E0EC37),
    TOBN(0xECD06E15, 0x57CD0915), TOBN(0xB7D2BBD2, 0xDF016199),
    TOBN(0xC8484B1E, 0x052588B9), TOBN(0xDB2A3B73, 0x13D3FE14),
    TOBN(0xD052B985, 0xD182EA0A), TOBN(0xA4BD1BFF, 0xE83B9C80),
    TOBN(0xDFC967C1, 0xFB3F2E55), TOBN(0xB5045AF2, 0x767164E1),
    TOBN(0x1D14348F, 0x6F2F9193), TOBN(0x64E67982, 0x428EBC83),
    TOBN(0x8AC376D2, 0x82D6ED38), TOBN(0x777DE62A, 0xAAB8A862),
    TOBN(0xDDF463E5, 0xE9EC144B), TOBN(0x0196F931, 0xC77A57F2),
    TOBN(0xA55AE313, 0x41000A65), TOBN(0x901228F8, 0xC28CBB18),
    TOBN(0xBC3773BF, 0x7E8C6F62), TOBN(0xBE3A6C1B, 0x0C6B47B1),
    TOBN(0xFF4FED4A, 0xAC0BB555), TOBN(0x10DBC150, 0x77BE463F),
    TOBN(0x07F4793A, 0x1A0BA125), TOBN(0x4CA7B18F, 0x21EF2054),
    TOBN(0x2E775066, 0x60EDBD48), TOBN(0x3FB32C9B, 0x73134D0B),
};
static const BN_ULONG dh2048_256_q[] = {
    TOBN(0xA308B0FE, 0x64F5FBD3), TOBN(0x99B1A47D, 0x1EB3750B),
    TOBN(0xB4479976, 0x40129DA2), TOBN(0x8CF83642, 0xA709A097),
};

struct standard_parameters {
  BIGNUM p, q, g;
};

static const struct standard_parameters dh1024_160 = {
  STATIC_BIGNUM(dh1024_160_p),
  STATIC_BIGNUM(dh1024_160_q),
  STATIC_BIGNUM(dh1024_160_g),
};

static const struct standard_parameters dh2048_224 = {
  STATIC_BIGNUM(dh2048_224_p),
  STATIC_BIGNUM(dh2048_224_q),
  STATIC_BIGNUM(dh2048_224_g),
};

static const struct standard_parameters dh2048_256 = {
  STATIC_BIGNUM(dh2048_256_p),
  STATIC_BIGNUM(dh2048_256_q),
  STATIC_BIGNUM(dh2048_256_g),
};

static DH *get_standard_parameters(const struct standard_parameters *params,
                                   const ENGINE *engine) {
  DH *dh = DH_new();
  if (!dh) {
    return NULL;
  }

  dh->p = BN_dup(&params->p);
  dh->q = BN_dup(&params->q);
  dh->g = BN_dup(&params->g);
  if (!dh->p || !dh->q || !dh->g) {
    DH_free(dh);
    return NULL;
  }

  return dh;
}

DH *DH_get_1024_160(const ENGINE *engine) {
  return get_standard_parameters(&dh1024_160, engine);
}

DH *DH_get_2048_224(const ENGINE *engine) {
  return get_standard_parameters(&dh2048_224, engine);
}

DH *DH_get_2048_256(const ENGINE *engine) {
  return get_standard_parameters(&dh2048_256, engine);
}

BIGNUM *BN_get_rfc3526_prime_1536(BIGNUM *ret) {
  static const BN_ULONG kPrime1536Data[] = {
      TOBN(0xffffffff, 0xffffffff), TOBN(0xf1746c08, 0xca237327),
      TOBN(0x670c354e, 0x4abc9804), TOBN(0x9ed52907, 0x7096966d),
      TOBN(0x1c62f356, 0x208552bb), TOBN(0x83655d23, 0xdca3ad96),
      TOBN(0x69163fa8, 0xfd24cf5f), TOBN(0x98da4836, 0x1c55d39a),
      TOBN(0xc2007cb8, 0xa163bf05), TOBN(0x49286651, 0xece45b3d),
      TOBN(0xae9f2411, 0x7c4b1fe6), TOBN(0xee386bfb, 0x5a899fa5),
      TOBN(0x0bff5cb6, 0xf406b7ed), TOBN(0xf44c42e9, 0xa637ed6b),
      TOBN(0xe485b576, 0x625e7ec6), TOBN(0x4fe1356d, 0x6d51c245),
      TOBN(0x302b0a6d, 0xf25f1437), TOBN(0xef9519b3, 0xcd3a431b),
      TOBN(0x514a0879, 0x8e3404dd), TOBN(0x020bbea6, 0x3b139b22),
      TOBN(0x29024e08, 0x8a67cc74), TOBN(0xc4c6628b, 0x80dc1cd1),
      TOBN(0xc90fdaa2, 0x2168c234), TOBN(0xffffffff, 0xffffffff),
  };

  static const BIGNUM kPrime1536BN = STATIC_BIGNUM(kPrime1536Data);

  BIGNUM *alloc = NULL;
  if (ret == NULL) {
    alloc = BN_new();
    if (alloc == NULL) {
      return NULL;
    }
    ret = alloc;
  }

  if (!BN_copy(ret, &kPrime1536BN)) {
    BN_free(alloc);
    return NULL;
  }

  return ret;
}
