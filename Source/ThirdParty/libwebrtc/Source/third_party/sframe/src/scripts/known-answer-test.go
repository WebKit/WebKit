package main

import (
	"crypto"
	"crypto/aes"
	"crypto/cipher"
	"crypto/hmac"
	"encoding/binary"
	"encoding/hex"
	"fmt"

	_ "crypto/sha256"
	_ "crypto/sha512"

	"golang.org/x/crypto/hkdf"
)

func chk(err error) {
	if err != nil {
		panic(err)
	}
}

func from_hex(s string) []byte {
	b, err := hex.DecodeString(s)
	chk(err)
	return b
}

type CipherSuite struct {
	ID      uint16
	Name    string
	Nk      int
	Nn      int
	Hash    crypto.Hash
	NewAEAD func(key []byte) cipher.AEAD
}

func newGCM(key []byte) cipher.AEAD {
	block, err := aes.NewCipher(key)
	chk(err)

	gcm, err := cipher.NewGCM(block)
	chk(err)

	return gcm
}

type AESCTRHMAC struct {
	Block   cipher.Block
	AuthKey []byte
	Hash    crypto.Hash
	TagSize int
}

func newAESCTRHMAC(key []byte, hash crypto.Hash, encSize, tagSize int) AESCTRHMAC {
	secret := hkdf.Extract(hash.New, key, []byte("SFrame10 AES CM AEAD"))

	encKey := make([]byte, encSize)
	hkdf.Expand(hash.New, secret, []byte("enc")).Read(encKey)

	authKey := make([]byte, hash.Size())
	hkdf.Expand(hash.New, secret, []byte("auth")).Read(authKey)

	block, err := aes.NewCipher(encKey)
	chk(err)

	return AESCTRHMAC{block, authKey, hash, tagSize}
}

func (ctr AESCTRHMAC) NonceSize() int {
	return 12
}

func (ctr AESCTRHMAC) Overhead() int {
	return ctr.TagSize
}

func (ctr AESCTRHMAC) crypt(nonce, pt []byte) []byte {
	iv := append(nonce, []byte{0, 0, 0, 0}...)
	stream := cipher.NewCTR(ctr.Block, iv)

	ct := make([]byte, len(pt))
	stream.XORKeyStream(ct, pt)
	return ct
}

func (ctr AESCTRHMAC) tag(aad, ct []byte) []byte {
	h := hmac.New(ctr.Hash.New, ctr.AuthKey)
	h.Write(aad)
	h.Write(ct)
	return h.Sum(nil)[:ctr.TagSize]
}

func (ctr AESCTRHMAC) Seal(dst, nonce, plaintext, additionalData []byte) []byte {
	ciphertext := ctr.crypt(nonce, plaintext)
	tag := ctr.tag(additionalData, ciphertext)
	return append(ciphertext, tag...)
}

func (ctr AESCTRHMAC) Open(dst, nonce, ciphertext, additionalData []byte) ([]byte, error) {
	cut := len(ciphertext) - ctr.TagSize
	innerCiphertext, tag := ciphertext[:cut], ciphertext[cut:]

	computedTag := ctr.tag(additionalData, innerCiphertext)
	if !hmac.Equal(computedTag, tag) {
		return nil, fmt.Errorf("Authentication failure")
	}

	plaintext := ctr.crypt(nonce, innerCiphertext)
	return plaintext, nil
}

func makeAESCTRHMAC(hash crypto.Hash, encSize, tagSize int) func(key []byte) cipher.AEAD {
	return func(key []byte) cipher.AEAD {
		return newAESCTRHMAC(key, hash, encSize, tagSize)
	}
}

var (
	AES_CM_128_HMAC_SHA256_4 = CipherSuite{
		ID:      0x0001,
		Name:    "AES_CM_128_HMAC_SHA256_4",
		Nk:      16,
		Nn:      12,
		Hash:    crypto.SHA256,
		NewAEAD: makeAESCTRHMAC(crypto.SHA256, 16, 4),
	}
	AES_CM_128_HMAC_SHA256_8 = CipherSuite{
		ID:      0x0002,
		Name:    "AES_CM_128_HMAC_SHA256_8",
		Nk:      16,
		Nn:      12,
		Hash:    crypto.SHA256,
		NewAEAD: makeAESCTRHMAC(crypto.SHA256, 16, 8),
	}
	AES_GCM_128_SHA256 = CipherSuite{
		ID:      0x0003,
		Name:    "AES_GCM_128_SHA256",
		Nk:      16,
		Nn:      12,
		Hash:    crypto.SHA256,
		NewAEAD: newGCM,
	}
	AES_GCM_256_SHA512 = CipherSuite{
		ID:      0x0004,
		Name:    "AES_GCM_256_SHA512",
		Nk:      32,
		Nn:      12,
		Hash:    crypto.SHA512,
		NewAEAD: newGCM,
	}
)

func (suite CipherSuite) Extract(ikm, salt []byte) []byte {
	return hkdf.Extract(suite.Hash.New, ikm, salt)
}

func (suite CipherSuite) Expand(prk, info []byte, size int) []byte {
	out := make([]byte, size)
	r := hkdf.Expand(suite.Hash.New, prk, info)
	r.Read(out)
	return out
}

// Cases:
// * KID=0x07, CTR=0, 1, 2
// * KID=0xffff, CTR=0
// * KID=0xffff, CTR=0x0100
var (
	baseKeys = map[uint16][]byte{
		AES_CM_128_HMAC_SHA256_4.ID: from_hex("101112131415161718191a1b1c1d1e1f"),
		AES_CM_128_HMAC_SHA256_8.ID: from_hex("202122232425262728292a2b2c2d2e2f"),
		AES_GCM_128_SHA256.ID:       from_hex("303132333435363738393a3b3c3d3e3f"),
		AES_GCM_256_SHA512.ID: from_hex("404142434445464748494a4b4c4d4e4f" +
			"505152535455565758595a5b5c5d5e5f"),
	}

	plaintext         = from_hex("00010203")
	header_7_0        = from_hex("0700")
	header_7_1        = from_hex("0701")
	header_7_2        = from_hex("0702")
	header_long_short = from_hex("0affff00")
	header_long_long  = from_hex("1affff0100")

	forSender = map[uint64]string{
		// Without context
		0xa:   "09a",
		0xaa:  "0a0aa",
		0xaaa: "0aaaa",

		// With context
		0xb00a:   "0b0b00a",
		0xbb00a:  "0bbb00a",
		0xbbb00a: "0c0bbb00a",

		0xb0aa:   "0b0b0aa",
		0xbb0aa:  "0bbb0aa",
		0xbbb0aa: "0c0bbb0aa",

		0xbaaa:   "0b0baaa",
		0xbbaaa:  "0bbbaaa",
		0xbbbaaa: "0c0bbbaaa",
	}
	epochSecret = map[int][]byte{
		0x00: from_hex("00000000000000000000000000000000"),
		0x0f: from_hex("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"),
		0x10: from_hex("10101010101010101010101010101010"),
	}
	epoch_mask = 0x0f
)

func protect(suite CipherSuite, baseKey []byte, ctr uint64, header []byte) []byte {
	secret := suite.Extract(baseKey, []byte("SFrame10"))
	key := suite.Expand(secret, []byte("key"), suite.Nk)
	salt := suite.Expand(secret, []byte("salt"), suite.Nn)

	nonce := make([]byte, suite.Nn)
	binary.BigEndian.PutUint64(nonce[suite.Nn-8:], ctr)
	for i := range nonce {
		nonce[i] ^= salt[i]
	}

	aead := suite.NewAEAD(key)
	ct := aead.Seal(nil, nonce, plaintext, header)
	return append(header, ct...)
}

func senderWithContext(sender, senderBits, context int) uint64 {
	return (uint64(context) << senderBits) | uint64(sender)
}

func mlsHeader(epoch, sender int) []byte {
	return mlsHeaderWithContext(epoch, sender, 32, 0)
}

func mlsHeaderWithContext(epoch, sender, senderBits, context int) []byte {
	epochStr := fmt.Sprintf("%01x", epoch&epoch_mask)
	senderIx := senderWithContext(sender, senderBits, context)
	return from_hex(forSender[senderIx] + epochStr + "00")
}

func mlsBaseKey(suite CipherSuite, epoch, sender int) []byte {
	return mlsBaseKeyWithContext(suite, epoch, sender, 32, 0)
}

func mlsBaseKeyWithContext(suite CipherSuite, epoch, sender, senderBits, context int) []byte {
	sframeEpochSecret := epochSecret[epoch]
	encSender := make([]byte, 8)
	binary.BigEndian.PutUint64(encSender, senderWithContext(sender, senderBits, context))
	return suite.Expand(sframeEpochSecret, encSender, suite.Hash.Size())
}

func mlsProtect(suite CipherSuite, epoch, sender int) []byte {
	return mlsProtectWithContext(suite, epoch, sender, 32, 0)
}

func mlsProtectWithContext(suite CipherSuite, epoch, sender, senderBits, context int) []byte {
	baseKey := mlsBaseKeyWithContext(suite, epoch, sender, senderBits, context)
	return protect(suite, baseKey, 0, mlsHeaderWithContext(epoch, sender, senderBits, context))
}

func main() {
	suites := []CipherSuite{
		AES_CM_128_HMAC_SHA256_4,
		AES_CM_128_HMAC_SHA256_8,
		AES_GCM_128_SHA256,
		AES_GCM_256_SHA512,
	}

	fmt.Printf("\n\n/* ***** ***** BASE ***** ***** */\n\n\n")

	for _, suite := range suites {
		ct_7_0 := protect(suite, baseKeys[suite.ID], 0, header_7_0)
		ct_7_1 := protect(suite, baseKeys[suite.ID], 1, header_7_1)
		ct_7_2 := protect(suite, baseKeys[suite.ID], 2, header_7_2)
		ct_long_short := protect(suite, baseKeys[suite.ID], 0, header_long_short)
		ct_long_long := protect(suite, baseKeys[suite.ID], 0x0100, header_long_long)

		fmt.Printf("{ CipherSuite::%s,\n", suite.Name)
		fmt.Printf("  {\n")
		fmt.Printf("    from_hex(\"%x\"),\n", baseKeys[suite.ID])
		fmt.Printf("    from_hex(\"%x\"),\n", ct_7_0)
		fmt.Printf("    from_hex(\"%x\"),\n", ct_7_1)
		fmt.Printf("    from_hex(\"%x\"),\n", ct_7_2)
		fmt.Printf("    from_hex(\"%x\"),\n", ct_long_short)
		fmt.Printf("    from_hex(\"%x\"),\n", ct_long_long)
		fmt.Printf("  } },\n")
	}

	fmt.Printf("\n\n/* ***** ***** MLS ***** ***** */\n\n\n")

	epochs := []int{0x00, 0x0f, 0x10}
	senders := []int{0xa, 0xaa, 0xaaa}

	for _, suite := range suites {
		fmt.Printf("{ CipherSuite::%s,\n", suite.Name)
		fmt.Printf("  { {\n")
		for _, epoch := range epochs {
			fmt.Printf("    {\n")
			for _, sender := range senders {
				ciphertext := mlsProtect(suite, epoch, sender)
				fmt.Printf("      from_hex(\"%x\"),\n", ciphertext)
			}
			fmt.Printf("    },\n")
		}
		fmt.Printf("  } } },\n")
	}

	fmt.Printf("\n\n/* ***** ***** MLS+CONTEXT ***** ***** */\n\n\n")

	contexts := []int{0xb, 0xbb, 0xbbb}
	senderBits := 12

	for _, suite := range suites {
		fmt.Printf("{ CipherSuite::%s,\n", suite.Name)
		fmt.Printf("  { {\n")
		for _, epoch := range epochs {
			fmt.Printf("    {\n")
			for _, sender := range senders {
				fmt.Printf("      {\n")
				for _, context := range contexts {
					ciphertext := mlsProtectWithContext(suite, epoch, sender, senderBits, context)
					fmt.Printf("        from_hex(\"%x\"),\n", ciphertext)
				}
				fmt.Printf("      },\n")
			}
			fmt.Printf("    },\n")
		}
		fmt.Printf("  } } },\n")
	}

}
