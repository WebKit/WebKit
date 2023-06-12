// Copyright (c) 2021, Google Inc.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

// testmodulewrapper is a modulewrapper binary that works with acvptool and
// implements the primitives that BoringSSL's modulewrapper doesn't, so that
// we have something that can exercise all the code in avcptool.

package main

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"

	"golang.org/x/crypto/hkdf"
	"golang.org/x/crypto/xts"
)

var (
	output       io.Writer
	outputBuffer *bytes.Buffer
)

var handlers = map[string]func([][]byte) error{
	"flush":                    flush,
	"getConfig":                getConfig,
	"KDF-counter":              kdfCounter,
	"AES-XTS/encrypt":          xtsEncrypt,
	"AES-XTS/decrypt":          xtsDecrypt,
	"HKDF/SHA2-256":            hkdfMAC,
	"hmacDRBG-reseed/SHA2-256": hmacDRBGReseed,
	"hmacDRBG-pr/SHA2-256":     hmacDRBGPredictionResistance,
	"AES-CBC-CS3/encrypt":      ctsEncrypt,
	"AES-CBC-CS3/decrypt":      ctsDecrypt,
}

func flush(args [][]byte) error {
	if outputBuffer == nil {
		return nil
	}

	if _, err := os.Stdout.Write(outputBuffer.Bytes()); err != nil {
		return err
	}
	outputBuffer = new(bytes.Buffer)
	output = outputBuffer
	return nil
}

func getConfig(args [][]byte) error {
	if len(args) != 0 {
		return fmt.Errorf("getConfig received %d args", len(args))
	}

	if err := reply([]byte(`[
	{
		"algorithm": "acvptool",
		"features": ["batch"]
	}, {
		"algorithm": "KDF",
		"revision": "1.0",
		"capabilities": [{
			"kdfMode": "counter",
			"macMode": [
				"HMAC-SHA2-256"
			],
			"supportedLengths": [{
				"min": 8,
				"max": 4096,
				"increment": 8
			}],
			"fixedDataOrder": [
				"before fixed data"
			],
			"counterLength": [
				32
			]
		}]
	}, {
		"algorithm": "ACVP-AES-XTS",
		"revision": "1.0",
		"direction": [
		  "encrypt",
		  "decrypt"
		],
		"keyLen": [
		  128,
		  256
		],
		"payloadLen": [
		  1024
		],
		"tweakMode": [
		  "number"
		]
	}, {
		"algorithm": "KDA",
		"mode": "HKDF",
		"revision": "Sp800-56Cr1",
		"fixedInfoPattern": "uPartyInfo||vPartyInfo",
		"encoding": [
			"concatenation"
		],
		"hmacAlg": [
			"SHA2-256"
		],
		"macSaltMethods": [
			"default",
			"random"
		],
		"l": 256,
		"z": [256, 384]
	}, {
		"algorithm": "hmacDRBG",
		"revision": "1.0",
		"predResistanceEnabled": [false, true],
		"reseedImplemented": true,
		"capabilities": [{
			"mode": "SHA2-256",
			"derFuncEnabled": false,
			"entropyInputLen": [
				256
			],
			"nonceLen": [
				128
			],
			"persoStringLen": [
				256
			],
			"additionalInputLen": [
				256
			],
			"returnedBitsLen": 256
		}]
	}, {
		"algorithm": "ACVP-AES-CBC-CS3",
		"revision": "1.0",
		"payloadLen": [{
			"min": 128,
			"max": 2048,
			"increment": 8
		}],
		"direction": [
		  "encrypt",
		  "decrypt"
		],
		"keyLen": [
		  128,
		  256
		]
	}
]`)); err != nil {
		return err
	}

	return flush(nil)
}

func kdfCounter(args [][]byte) error {
	if len(args) != 5 {
		return fmt.Errorf("KDF received %d args", len(args))
	}

	outputBytes32, prf, counterLocation, key, counterBits32 := args[0], args[1], args[2], args[3], args[4]
	outputBytes := binary.LittleEndian.Uint32(outputBytes32)
	counterBits := binary.LittleEndian.Uint32(counterBits32)

	if !bytes.Equal(prf, []byte("HMAC-SHA2-256")) {
		return fmt.Errorf("KDF received unsupported PRF %q", string(prf))
	}
	if !bytes.Equal(counterLocation, []byte("before fixed data")) {
		return fmt.Errorf("KDF received unsupported counter location %q", counterLocation)
	}
	if counterBits != 32 {
		return fmt.Errorf("KDF received unsupported counter length %d", counterBits)
	}

	if len(key) == 0 {
		key = make([]byte, 32)
		rand.Reader.Read(key)
	}

	// See https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-108.pdf section 5.1
	if outputBytes+31 < outputBytes {
		return fmt.Errorf("KDF received excessive output length %d", outputBytes)
	}

	n := (outputBytes + 31) / 32
	result := make([]byte, 0, 32*n)
	mac := hmac.New(sha256.New, key)
	var input [4 + 8]byte
	var digest []byte
	rand.Reader.Read(input[4:])
	for i := uint32(1); i <= n; i++ {
		mac.Reset()
		binary.BigEndian.PutUint32(input[:4], i)
		mac.Write(input[:])
		digest = mac.Sum(digest[:0])
		result = append(result, digest...)
	}

	return reply(key, input[4:], result[:outputBytes])
}

func reply(responses ...[]byte) error {
	if len(responses) > maxArgs {
		return fmt.Errorf("%d responses is too many", len(responses))
	}

	var lengths [4 * (1 + maxArgs)]byte
	binary.LittleEndian.PutUint32(lengths[:4], uint32(len(responses)))
	for i, response := range responses {
		binary.LittleEndian.PutUint32(lengths[4*(i+1):4*(i+2)], uint32(len(response)))
	}

	lengthsLength := (1 + len(responses)) * 4
	if n, err := output.Write(lengths[:lengthsLength]); n != lengthsLength || err != nil {
		return fmt.Errorf("write failed: %s", err)
	}

	for _, response := range responses {
		if n, err := output.Write(response); n != len(response) || err != nil {
			return fmt.Errorf("write failed: %s", err)
		}
	}

	return nil
}

func xtsEncrypt(args [][]byte) error {
	return doXTS(args, false)
}

func xtsDecrypt(args [][]byte) error {
	return doXTS(args, true)
}

func doXTS(args [][]byte, decrypt bool) error {
	if len(args) != 3 {
		return fmt.Errorf("XTS received %d args, wanted 3", len(args))
	}
	key := args[0]
	msg := args[1]
	tweak := args[2]

	if len(msg)%16 != 0 {
		return fmt.Errorf("XTS received %d-byte msg, need multiple of 16", len(msg))
	}
	if len(tweak) != 16 {
		return fmt.Errorf("XTS received %d-byte tweak, wanted 16", len(tweak))
	}

	var zeros [8]byte
	if !bytes.Equal(tweak[8:], zeros[:]) {
		return errors.New("XTS received tweak with invalid structure. Ensure that configuration specifies a 'number' tweak")
	}

	sectorNum := binary.LittleEndian.Uint64(tweak[:8])

	c, err := xts.NewCipher(aes.NewCipher, key)
	if err != nil {
		return err
	}

	if decrypt {
		c.Decrypt(msg, msg, sectorNum)
	} else {
		c.Encrypt(msg, msg, sectorNum)
	}

	return reply(msg)
}

func hkdfMAC(args [][]byte) error {
	if len(args) != 4 {
		return fmt.Errorf("HKDF received %d args, wanted 4", len(args))
	}

	key := args[0]
	salt := args[1]
	info := args[2]
	lengthBytes := args[3]

	if len(lengthBytes) != 4 {
		return fmt.Errorf("uint32 length was %d bytes long", len(lengthBytes))
	}

	length := binary.LittleEndian.Uint32(lengthBytes)

	mac := hkdf.New(sha256.New, key, salt, info)
	ret := make([]byte, length)
	mac.Read(ret)

	return reply(ret)
}

func hmacDRBGReseed(args [][]byte) error {
	if len(args) != 8 {
		return fmt.Errorf("hmacDRBG received %d args, wanted 8", len(args))
	}

	outLenBytes, entropy, personalisation, reseedAdditionalData, reseedEntropy, additionalData1, additionalData2, nonce := args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]

	if len(outLenBytes) != 4 {
		return fmt.Errorf("uint32 length was %d bytes long", len(outLenBytes))
	}
	outLen := binary.LittleEndian.Uint32(outLenBytes)
	out := make([]byte, outLen)

	drbg := NewHMACDRBG(entropy, nonce, personalisation)
	drbg.Reseed(reseedEntropy, reseedAdditionalData)
	drbg.Generate(out, additionalData1)
	drbg.Generate(out, additionalData2)

	return reply(out)
}

func hmacDRBGPredictionResistance(args [][]byte) error {
	if len(args) != 8 {
		return fmt.Errorf("hmacDRBG received %d args, wanted 8", len(args))
	}

	outLenBytes, entropy, personalisation, additionalData1, entropy1, additionalData2, entropy2, nonce := args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]

	if len(outLenBytes) != 4 {
		return fmt.Errorf("uint32 length was %d bytes long", len(outLenBytes))
	}
	outLen := binary.LittleEndian.Uint32(outLenBytes)
	out := make([]byte, outLen)

	drbg := NewHMACDRBG(entropy, nonce, personalisation)
	drbg.Reseed(entropy1, additionalData1)
	drbg.Generate(out, nil)
	drbg.Reseed(entropy2, additionalData2)
	drbg.Generate(out, nil)

	return reply(out)
}

func swapFinalTwoAESBlocks(d []byte) {
	var blockNMinus1 [aes.BlockSize]byte
	copy(blockNMinus1[:], d[len(d)-2*aes.BlockSize:])
	copy(d[len(d)-2*aes.BlockSize:], d[len(d)-aes.BlockSize:])
	copy(d[len(d)-aes.BlockSize:], blockNMinus1[:])
}

func roundUp(n, m int) int {
	return n + (m-(n%m))%m
}

func doCTSEncrypt(key, origPlaintext, iv []byte) []byte {
	// https://nvlpubs.nist.gov/nistpubs/legacy/sp/nistspecialpublication800-38a-add.pdf
	if len(origPlaintext) < aes.BlockSize {
		panic("input too small")
	}

	plaintext := make([]byte, roundUp(len(origPlaintext), aes.BlockSize))
	copy(plaintext, origPlaintext)

	block, err := aes.NewCipher(key)
	if err != nil {
		panic(err)
	}
	cbcEncryptor := cipher.NewCBCEncrypter(block, iv)
	cbcEncryptor.CryptBlocks(plaintext, plaintext)
	ciphertext := plaintext

	if len(origPlaintext) > aes.BlockSize {
		swapFinalTwoAESBlocks(ciphertext)

		if len(origPlaintext)%16 != 0 {
			// Truncate the ciphertext
			ciphertext = ciphertext[:len(ciphertext)-aes.BlockSize+(len(origPlaintext)%aes.BlockSize)]
		}
	}

	if len(ciphertext) != len(origPlaintext) {
		panic("internal error")
	}

	return ciphertext
}

func doCTSDecrypt(key, origCiphertext, iv []byte) []byte {
	if len(origCiphertext) < aes.BlockSize {
		panic("input too small")
	}

	ciphertext := make([]byte, roundUp(len(origCiphertext), aes.BlockSize))
	copy(ciphertext, origCiphertext)

	if len(ciphertext) > aes.BlockSize {
		swapFinalTwoAESBlocks(ciphertext)
	}

	block, err := aes.NewCipher(key)
	if err != nil {
		panic(err)
	}
	cbcDecrypter := cipher.NewCBCDecrypter(block, iv)

	var plaintext []byte
	if overhang := len(origCiphertext) % aes.BlockSize; overhang == 0 {
		cbcDecrypter.CryptBlocks(ciphertext, ciphertext)
		plaintext = ciphertext
	} else {
		ciphertext, finalBlock := ciphertext[:len(ciphertext)-aes.BlockSize], ciphertext[len(ciphertext)-aes.BlockSize:]
		var plaintextFinalBlock [aes.BlockSize]byte
		block.Decrypt(plaintextFinalBlock[:], finalBlock)
		copy(ciphertext[len(ciphertext)-aes.BlockSize+overhang:], plaintextFinalBlock[overhang:])
		plaintext = make([]byte, len(origCiphertext))
		cbcDecrypter.CryptBlocks(plaintext, ciphertext)
		for i := 0; i < overhang; i++ {
			plaintextFinalBlock[i] ^= ciphertext[len(ciphertext)-aes.BlockSize+i]
		}
		copy(plaintext[len(ciphertext):], plaintextFinalBlock[:overhang])
	}

	return plaintext
}

func ctsEncrypt(args [][]byte) error {
	if len(args) != 4 {
		return fmt.Errorf("ctsEncrypt received %d args, wanted 4", len(args))
	}

	key, plaintext, iv, numIterations32 := args[0], args[1], args[2], args[3]
	if len(numIterations32) != 4 || binary.LittleEndian.Uint32(numIterations32) != 1 {
		return errors.New("only a single iteration supported for ctsEncrypt")
	}

	if len(plaintext) < aes.BlockSize {
		return fmt.Errorf("ctsEncrypt plaintext too short: %d bytes", len(plaintext))
	}

	return reply(doCTSEncrypt(key, plaintext, iv))
}

func ctsDecrypt(args [][]byte) error {
	if len(args) != 4 {
		return fmt.Errorf("ctsDecrypt received %d args, wanted 4", len(args))
	}

	key, ciphertext, iv, numIterations32 := args[0], args[1], args[2], args[3]
	if len(numIterations32) != 4 || binary.LittleEndian.Uint32(numIterations32) != 1 {
		return errors.New("only a single iteration supported for ctsDecrypt")
	}

	if len(ciphertext) < aes.BlockSize {
		return errors.New("ctsDecrypt ciphertext too short")
	}

	return reply(doCTSDecrypt(key, ciphertext, iv))
}

const (
	maxArgs       = 9
	maxArgLength  = 1 << 20
	maxNameLength = 30
)

func main() {
	if err := do(); err != nil {
		fmt.Fprintf(os.Stderr, "%s.\n", err)
		os.Exit(1)
	}
}

func do() error {
	// In order to exercise pipelining, all output is buffered until a "flush".
	outputBuffer = new(bytes.Buffer)
	output = outputBuffer

	var nums [4 * (1 + maxArgs)]byte
	var argLengths [maxArgs]uint32
	var args [maxArgs][]byte
	var argsData []byte

	for {
		if _, err := io.ReadFull(os.Stdin, nums[:8]); err != nil {
			return err
		}

		numArgs := binary.LittleEndian.Uint32(nums[:4])
		if numArgs == 0 {
			return errors.New("Invalid, zero-argument operation requested")
		} else if numArgs > maxArgs {
			return fmt.Errorf("Operation requested with %d args, but %d is the limit", numArgs, maxArgs)
		}

		if numArgs > 1 {
			if _, err := io.ReadFull(os.Stdin, nums[8:4+4*numArgs]); err != nil {
				return err
			}
		}

		input := nums[4:]
		var need uint64
		for i := uint32(0); i < numArgs; i++ {
			argLength := binary.LittleEndian.Uint32(input[:4])
			if i == 0 && argLength > maxNameLength {
				return fmt.Errorf("Operation with name of length %d exceeded limit of %d", argLength, maxNameLength)
			} else if argLength > maxArgLength {
				return fmt.Errorf("Operation with argument of length %d exceeded limit of %d", argLength, maxArgLength)
			}
			need += uint64(argLength)
			argLengths[i] = argLength
			input = input[4:]
		}

		if need > uint64(cap(argsData)) {
			argsData = make([]byte, need)
		} else {
			argsData = argsData[:need]
		}

		if _, err := io.ReadFull(os.Stdin, argsData); err != nil {
			return err
		}

		input = argsData
		for i := uint32(0); i < numArgs; i++ {
			args[i] = input[:argLengths[i]]
			input = input[argLengths[i]:]
		}

		name := string(args[0])
		if handler, ok := handlers[name]; !ok {
			return fmt.Errorf("unknown operation %q", name)
		} else {
			if err := handler(args[1:numArgs]); err != nil {
				return err
			}
		}
	}
}
