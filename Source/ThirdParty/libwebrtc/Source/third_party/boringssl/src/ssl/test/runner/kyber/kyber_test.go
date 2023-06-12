/* Copyright (c) 2023, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

package kyber

import (
	"bufio"
	"bytes"
	"encoding/hex"
	"golang.org/x/crypto/sha3"
	"os"
	"strings"
	"testing"
)

func TestVectors(t *testing.T) {
	in, err := os.Open("../../../../crypto/kyber/kyber_tests.txt")
	if err != nil {
		t.Error(err)
		return
	}
	defer in.Close()

	scanner := bufio.NewScanner(in)
	var priv *PrivateKey
	var encodedPublicKey *[PublicKeySize]byte
	var ciphertext *[CiphertextSize]byte
	sharedSecret := make([]byte, 32)

	lineNo := 0
	for scanner.Scan() {
		lineNo++
		line := scanner.Text()

		parts := strings.Split(line, "=")
		if len(parts) != 2 || strings.HasPrefix(line, "count ") {
			continue
		}
		key := strings.TrimSpace(parts[0])
		value, err := hex.DecodeString(strings.TrimSpace(parts[1]))
		if err != nil {
			t.Errorf("bad hex value on line %d: %q", lineNo, parts[1])
			return
		}

		switch key {
		case "generateEntropy":
			priv, encodedPublicKey = NewPrivateKey((*[64]byte)(value))
		case "encapEntropyPreHash":
			hashedEntropy := sha3.Sum256(value)
			ciphertext = priv.Encap(sharedSecret, &hashedEntropy)
			decapSharedSecret := make([]byte, len(sharedSecret))
			priv.Decap(decapSharedSecret, ciphertext)
			if !bytes.Equal(sharedSecret, decapSharedSecret) {
				t.Errorf("instance on line %d did not round trip", lineNo)
				return
			}
		case "pk":
			if !bytes.Equal(encodedPublicKey[:], value) {
				t.Errorf("bad 'pk' value on line %d:\nwant: %x\ncalc: %x", lineNo, value, encodedPublicKey)
				return
			}
		case "sk":
			encodedPrivateKey := priv.Marshal()
			if !bytes.Equal(encodedPrivateKey[:], value) {
				t.Errorf("bad 'sk' value on line %d:\nwant: %x\ncalc: %x", lineNo, value, encodedPrivateKey)
				return
			}
		case "ct":
			if !bytes.Equal(ciphertext[:], value) {
				t.Errorf("bad 'ct' value on line %d:\nwant: %x\ncalc: %x", lineNo, value, ciphertext[:])
				return
			}
		case "ss":
			if !bytes.Equal(sharedSecret[:], value) {
				t.Errorf("bad 'ss' value on line %d:\nwant: %x\ncalc: %x", lineNo, value, sharedSecret[:])
				return
			}
		}
	}
}

func TestIteration(t *testing.T) {
	h := sha3.NewShake256()

	for i := 0; i < 4096; i++ {
		var generateEntropy [64]byte
		h.Read(generateEntropy[:])
		var encapEntropy [32]byte
		h.Read(encapEntropy[:])

		priv, encodedPublicKey := NewPrivateKey(&generateEntropy)
		h.Reset()
		h.Write(encodedPublicKey[:])
		encodedPrivateKey := priv.Marshal()
		h.Write(encodedPrivateKey[:])

		var sharedSecret [32]byte
		ciphertext := priv.Encap(sharedSecret[:], &encapEntropy)
		h.Write(ciphertext[:])
		h.Write(sharedSecret[:])

		var decapSharedSecret [32]byte
		priv.Decap(decapSharedSecret[:], ciphertext)
		if !bytes.Equal(decapSharedSecret[:], sharedSecret[:]) {
			t.Errorf("Decap failed on iteration %d", i)
			return
		}
	}

	var result [16]byte
	h.Read(result[:])
	const expected = "18c6cd04eaebb33b20bb1e8e2762d30d"
	if hex.EncodeToString(result[:]) != expected {
		t.Errorf("iteration test produced %x, but should be %v", result, expected)
	}
}
