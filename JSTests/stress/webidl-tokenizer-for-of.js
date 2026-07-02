// These regular expressions use the sticky flag so they will only match at
// the current location (ie. the offset of lastIndex).
const tokenRe = {
  // This expression uses a lookahead assertion to catch false matches
  // against integers early.
  "decimal": /-?(?=[0-9]*\.|[0-9]+[eE])(([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)([Ee][-+]?[0-9]+)?|[0-9]+[Ee][-+]?[0-9]+)/y,
  "integer": /-?(0([Xx][0-9A-Fa-f]+|[0-7]*)|[1-9][0-9]*)/y,
  "identifier": /[_-]?[A-Za-z][0-9A-Z_a-z-]*/y,
  "string": /"[^"]*"/y,
  "whitespace": /[\t\n\r ]+/y,
  "comment": /((\/(\/.*|\*([^*]|\*[^/])*\*\/)[\t\n\r ]*)+)/y,
  "other": /[^\t\n\r 0-9A-Za-z]/y
};

const stringTypes = [
  "ByteString",
  "DOMString",
  "USVString"
];
"use strict";

const argumentNameKeywords = [
  "async",
  "attribute",
  "callback",
  "const",
  "constructor",
  "deleter",
  "dictionary",
  "enum",
  "getter",
  "includes",
  "inherit",
  "interface",
  "iterable",
  "maplike",
  "namespace",
  "partial",
  "required",
  "setlike",
  "setter",
  "static",
  "stringifier",
  "typedef",
  "unrestricted"
];

const nonRegexTerminals = [
  "-Infinity",
  "FrozenArray",
  "Infinity",
  "NaN",
  "Promise",
  "async",
  "boolean",
  "byte",
  "constructor",
  "double",
  "false",
  "float",
  "long",
  "mixin",
  "null",
  "octet",
  "optional",
  "or",
  "readonly",
  "record",
  "sequence",
  "short",
  "true",
  "unsigned",
  "void"
].concat(argumentNameKeywords, stringTypes);

const punctuations = [
  "(",
  ")",
  ",",
  "...",
  ":",
  ";",
  "<",
  "=",
  ">",
  "?",
  "[",
  "]",
  "{",
  "}"
];

const reserved = [
  // "constructor" is now a keyword
  "_constructor",
  "toString",
  "_toString",
];

/**
 * @param {string} str
 */
function tokenise(str) {
  const tokens = [];
  let lastCharIndex = 0;
  let trivia = "";
  let line = 1;
  let index = 0;
  while (lastCharIndex < str.length) {
    const nextChar = str.charAt(lastCharIndex);
    let result = -1;

    if (/[\t\n\r ]/.test(nextChar)) {
      result = attemptTokenMatch("whitespace", { noFlushTrivia: true });
    } else if (nextChar === '/') {
      result = attemptTokenMatch("comment", { noFlushTrivia: true });
    }

    if (result !== -1) {
      const currentTrivia = tokens.pop().value;
      line += (currentTrivia.match(/\n/g) || []).length;
      trivia += currentTrivia;
      index -= 1;
    } else if (/[-0-9.A-Z_a-z]/.test(nextChar)) {
      result = attemptTokenMatch("decimal");
      if (result === -1) {
        result = attemptTokenMatch("integer");
      }
      if (result === -1) {
        result = attemptTokenMatch("identifier");
        const lastIndex = tokens.length - 1;
        const token = tokens[lastIndex];
        if (result !== -1) {
          if (reserved.includes(token.value)) {
            const message = `${Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(token.value)} is a reserved identifier and must not be used.`;
            throw new WebIDLParseError(Object(_error_js__WEBPACK_IMPORTED_MODULE_0__["syntaxError"])(tokens, lastIndex, null, message));
          } else if (nonRegexTerminals.includes(token.value)) {
            token.type = token.value;
          }
        }
      }
    } else if (nextChar === '"') {
      result = attemptTokenMatch("string");
    }

    for (const punctuation of punctuations) {
      if (str.startsWith(punctuation, lastCharIndex)) {
        tokens.push({ type: punctuation, value: punctuation, trivia, line, index });
        trivia = "";
        lastCharIndex += punctuation.length;
        result = lastCharIndex;
        break;
      }
    }

    // other as the last try
    if (result === -1) {
      result = attemptTokenMatch("other");
    }
    if (result === -1) {
      throw new Error("Token stream not progressing");
    }
    lastCharIndex = result;
    index += 1;
  }

  // remaining trivia as eof
  tokens.push({
    type: "eof",
    value: "",
    trivia
  });

  return tokens;

  /**
   * @param {keyof tokenRe} type
   * @param {object} [options]
   * @param {boolean} [options.noFlushTrivia]
   */
  function attemptTokenMatch(type, { noFlushTrivia } = {}) {
    const re = tokenRe[type];
    re.lastIndex = lastCharIndex;
    const result = re.exec(str);
    if (result) {
      tokens.push({ type, value: result[0], trivia, line, index });
      if (!noFlushTrivia) {
        trivia = "";
      }
      return re.lastIndex;
    }
    return -1;
  }
}


class Tokeniser {
  /**
   * @param {string} idl
   */
  constructor(idl) {
    this.source = tokenise(idl);
    this.position = 0;
  }

  /**
   * @param {string} type
   */
  probe(type) {
    return this.source.length > this.position && this.source[this.position].type === type;
  }

  /**
   * @param  {...string} candidates
   */
  consume(...candidates) {
    for (const type of candidates) {
      if (!this.probe(type)) continue;
      const token = this.source[this.position];
      this.position++;
      return token;
    }
  }

  /**
   * @param {number} position
   */
  unconsume(position) {
    this.position = position;
  }
}

noInline(Tokeniser.prototype.consume);

let content = `// Content was automatically extracted by Reffy into reffy-reports
// (https://github.com/tidoust/reffy-reports)
// Source: Web Cryptography API (https://w3c.github.io/webcrypto/Overview.html)

partial interface mixin WindowOrWorkerGlobalScope {
  [SameObject] readonly attribute Crypto crypto;
};

[Exposed=(Window,Worker)]
interface Crypto {
  [SecureContext] readonly attribute SubtleCrypto subtle;
  ArrayBufferView getRandomValues(ArrayBufferView array);
};

typedef (object or DOMString) AlgorithmIdentifier;

typedef AlgorithmIdentifier HashAlgorithmIdentifier;

dictionary Algorithm {
  required DOMString name;
};

dictionary KeyAlgorithm {
  required DOMString name;
};

enum KeyType { "public", "private", "secret" };

enum KeyUsage { "encrypt", "decrypt", "sign", "verify", "deriveKey", "deriveBits", "wrapKey", "unwrapKey" };

[SecureContext,Exposed=(Window,Worker)]
interface CryptoKey {
  readonly attribute KeyType type;
  readonly attribute boolean extractable;
  readonly attribute object algorithm;
  readonly attribute object usages;
};

enum KeyFormat { "raw", "spki", "pkcs8", "jwk" };

[SecureContext,Exposed=(Window,Worker)]
interface SubtleCrypto {
  Promise<any> encrypt(AlgorithmIdentifier algorithm,
                       CryptoKey key,
                       BufferSource data);
  Promise<any> decrypt(AlgorithmIdentifier algorithm,
                       CryptoKey key,
                       BufferSource data);
  Promise<any> sign(AlgorithmIdentifier algorithm,
                    CryptoKey key,
                    BufferSource data);
  Promise<any> verify(AlgorithmIdentifier algorithm,
                      CryptoKey key,
                      BufferSource signature,
                      BufferSource data);
  Promise<any> digest(AlgorithmIdentifier algorithm,
                      BufferSource data);

  Promise<any> generateKey(AlgorithmIdentifier algorithm,
                          boolean extractable,
                          sequence<KeyUsage> keyUsages );
  Promise<any> deriveKey(AlgorithmIdentifier algorithm,
                         CryptoKey baseKey,
                         AlgorithmIdentifier derivedKeyType,
                         boolean extractable,
                         sequence<KeyUsage> keyUsages );
  Promise<ArrayBuffer> deriveBits(AlgorithmIdentifier algorithm,
                          CryptoKey baseKey,
                          unsigned long length);

  Promise<CryptoKey> importKey(KeyFormat format,
                         (BufferSource or JsonWebKey) keyData,
                         AlgorithmIdentifier algorithm,
                         boolean extractable,
                         sequence<KeyUsage> keyUsages );
  Promise<any> exportKey(KeyFormat format, CryptoKey key);

  Promise<any> wrapKey(KeyFormat format,
                       CryptoKey key,
                       CryptoKey wrappingKey,
                       AlgorithmIdentifier wrapAlgorithm);
  Promise<CryptoKey> unwrapKey(KeyFormat format,
                         BufferSource wrappedKey,
                         CryptoKey unwrappingKey,
                         AlgorithmIdentifier unwrapAlgorithm,
                         AlgorithmIdentifier unwrappedKeyAlgorithm,
                         boolean extractable,
                         sequence<KeyUsage> keyUsages );
};

dictionary RsaOtherPrimesInfo {
  // The following fields are defined in Section 6.3.2.7 of JSON Web Algorithms
  DOMString r;
  DOMString d;
  DOMString t;
};

dictionary JsonWebKey {
  // The following fields are defined in Section 3.1 of JSON Web Key
  DOMString kty;
  DOMString use;
  sequence<DOMString> key_ops;
  DOMString alg;

  // The following fields are defined in JSON Web Key Parameters Registration
  boolean ext;

  // The following fields are defined in Section 6 of JSON Web Algorithms
  DOMString crv;
  DOMString x;
  DOMString y;
  DOMString d;
  DOMString n;
  DOMString e;
  DOMString p;
  DOMString q;
  DOMString dp;
  DOMString dq;
  DOMString qi;
  sequence<RsaOtherPrimesInfo> oth;
  DOMString k;
};

typedef Uint8Array BigInteger;

dictionary CryptoKeyPair {
  CryptoKey publicKey;
  CryptoKey privateKey;
};

dictionary RsaKeyGenParams : Algorithm {
  // The length, in bits, of the RSA modulus
  required [EnforceRange] unsigned long modulusLength;
  // The RSA public exponent
  required BigInteger publicExponent;
};

dictionary RsaHashedKeyGenParams : RsaKeyGenParams {
  // The hash algorithm to use
  required HashAlgorithmIdentifier hash;
};

dictionary RsaKeyAlgorithm : KeyAlgorithm {
  // The length, in bits, of the RSA modulus
  required unsigned long modulusLength;
  // The RSA public exponent
  required BigInteger publicExponent;
};

dictionary RsaHashedKeyAlgorithm : RsaKeyAlgorithm {
  // The hash algorithm that is used with this key
  required KeyAlgorithm hash;
};

dictionary RsaHashedImportParams : Algorithm {
  // The hash algorithm to use
  required HashAlgorithmIdentifier hash;
};

dictionary RsaPssParams : Algorithm {
  // The desired length of the random salt
  required [EnforceRange] unsigned long saltLength;
};

dictionary RsaOaepParams : Algorithm {
  // The optional label/application data to associate with the message
  BufferSource label;
};

dictionary EcdsaParams : Algorithm {
  // The hash algorithm to use
  required HashAlgorithmIdentifier hash;
};

typedef DOMString NamedCurve;

dictionary EcKeyGenParams : Algorithm {
  // A named curve
  required NamedCurve namedCurve;
};

dictionary EcKeyAlgorithm : KeyAlgorithm {
  // The named curve that the key uses
  required NamedCurve namedCurve;
};

dictionary EcKeyImportParams : Algorithm {
  // A named curve
  required NamedCurve namedCurve;
};

dictionary EcdhKeyDeriveParams : Algorithm {
  // The peer's EC public key.
  required CryptoKey public;
};

dictionary AesCtrParams : Algorithm {
  // The initial value of the counter block. counter MUST be 16 bytes
  // (the AES block size). The counter bits are the rightmost length
  // bits of the counter block. The rest of the counter block is for
  // the nonce. The counter bits are incremented using the standard
  // incrementing function specified in NIST SP 800-38A Appendix B.1:
  // the counter bits are interpreted as a big-endian integer and
  // incremented by one.
  required BufferSource counter;
  // The length, in bits, of the rightmost part of the counter block
  // that is incremented.
  required [EnforceRange] octet length;
};

dictionary AesKeyAlgorithm : KeyAlgorithm {
  // The length, in bits, of the key.
  required unsigned short length;
};

dictionary AesKeyGenParams : Algorithm {
  // The length, in bits, of the key.
  required [EnforceRange] unsigned short length;
};

dictionary AesDerivedKeyParams : Algorithm {
  // The length, in bits, of the key.
  required [EnforceRange] unsigned short length;
};

dictionary AesCbcParams : Algorithm {
  // The initialization vector. MUST be 16 bytes.
  required BufferSource iv;
};

dictionary AesGcmParams : Algorithm {
  // The initialization vector to use. May be up to 2^64-1 bytes long.
  required BufferSource iv;
  // The additional authentication data to include.
  BufferSource additionalData;
  // The desired length of the authentication tag. May be 0 - 128.
  [EnforceRange] octet tagLength;
};

dictionary HmacImportParams : Algorithm {
  // The inner hash function to use.
  required HashAlgorithmIdentifier hash;
  // The length (in bits) of the key.
  [EnforceRange] unsigned long length;
};

dictionary HmacKeyAlgorithm : KeyAlgorithm {
  // The inner hash function to use.
  required KeyAlgorithm hash;
  // The length (in bits) of the key.
  required unsigned long length;
};

dictionary HmacKeyGenParams : Algorithm {
  // The inner hash function to use.
  required HashAlgorithmIdentifier hash;
  // The length (in bits) of the key to generate. If unspecified, the
  // recommended length will be used, which is the size of the associated hash function's block
  // size.
  [EnforceRange] unsigned long length;
};

dictionary HkdfParams : Algorithm {
  // The algorithm to use with HMAC (e.g.: SHA-256)
  required HashAlgorithmIdentifier hash;
  // A bit string that corresponds to the salt used in the extract step.
  required BufferSource salt;
  // A bit string that corresponds to the context and application specific context for the derived keying material.
  required BufferSource info;
};

dictionary Pbkdf2Params : Algorithm {
  required BufferSource salt;
  required [EnforceRange] unsigned long iterations;
  required HashAlgorithmIdentifier hash;
};`

let tokeniser = new Tokeniser(content);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["mixin"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "async", "required"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["identifier", "decimal", "integer", "string"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["mixin"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["maplike", "setlike"]);
tokeniser.unconsume(30);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "async", "required"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(35);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(35);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["enum"]);
tokeniser.consume(...["typedef"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["or"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["or"]);
tokeniser.consume(...[")"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["enum"]);
tokeniser.consume(...["typedef"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["enum"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["enum"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["identifier", "decimal", "integer", "string"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["mixin"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["maplike", "setlike"]);
tokeniser.unconsume(118);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "async", "required"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["maplike", "setlike"]);
tokeniser.unconsume(123);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "async", "required"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["maplike", "setlike"]);
tokeniser.unconsume(128);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "async", "required"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["maplike", "setlike"]);
tokeniser.unconsume(133);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "async", "required"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["enum"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["string"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["identifier", "decimal", "integer", "string"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["mixin"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(166);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(166);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(182);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(182);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(198);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(198);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(214);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(214);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(233);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(233);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(246);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(246);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(265);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(265);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(290);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(290);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(307);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(307);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["or"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["or"]);
tokeniser.consume(...[")"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(336);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(336);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(349);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(349);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["const"]);
tokeniser.consume(...["constructor"]);
tokeniser.consume(...["static"]);
tokeniser.consume(...["stringifier"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["async"]);
tokeniser.consume(...["iterable", "maplike", "setlike"]);
tokeniser.unconsume(368);
tokeniser.consume(...["inherit"]);
tokeniser.consume(...["readonly"]);
tokeniser.consume(...["attribute"]);
tokeniser.unconsume(368);
tokeniser.consume(...["getter", "setter", "deleter"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier", "includes"]);
tokeniser.consume(...["("]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...["["]);
tokeniser.consume(...["optional"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["..."]);
tokeniser.consume(...["identifier", "async", "attribute", "callback", "const", "constructor", "deleter", "dictionary", "enum", "getter", "includes", "inherit", "interface", "iterable", "maplike", "namespace", "partial", "required", "setlike", "setter", "static", "stringifier", "typedef", "unrestricted"]);
tokeniser.consume(...[","]);
tokeniser.consume(...[")"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["<"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...[">"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["enum"]);
tokeniser.consume(...["typedef"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["enum"]);
tokeniser.consume(...["typedef"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...[":"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["{"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...["("]);
tokeniser.consume(...[","]);
tokeniser.consume(...["]"]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["long"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["["]);
tokeniser.consume(...["required"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["FrozenArray", "Promise", "sequence", "record"]);
tokeniser.consume(...["unsigned"]);
tokeniser.consume(...["short", "long"]);
tokeniser.consume(...["unrestricted"]);
tokeniser.consume(...["float", "double"]);
tokeniser.consume(...["boolean", "byte", "octet"]);
tokeniser.consume(...["identifier", "ByteString", "DOMString", "USVString"]);
tokeniser.consume(...["?"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["="]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["}"]);
tokeniser.consume(...[";"]);
tokeniser.consume(...["["]);
tokeniser.consume(...["callback"]);
tokeniser.consume(...["interface"]);
tokeniser.consume(...["partial"]);
tokeniser.consume(...["dictionary"]);
tokeniser.consume(...["enum"]);
tokeniser.consume(...["typedef"]);
tokeniser.consume(...["identifier"]);
tokeniser.consume(...["namespace"]);
tokeniser.consume(...["eof"]);
