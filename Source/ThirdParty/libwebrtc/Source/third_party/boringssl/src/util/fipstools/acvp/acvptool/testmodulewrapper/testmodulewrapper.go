// testmodulewrapper is a modulewrapper binary that works with acvptool and
// implements the primitives that BoringSSL's modulewrapper doesn't, so that
// we have something that can exercise all the code in avcptool.

package main

import (
	"bytes"
	"crypto/aes"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"

	"golang.org/x/crypto/xts"
)

var handlers = map[string]func([][]byte) error{
	"getConfig":       getConfig,
	"KDF-counter":     kdfCounter,
	"AES-XTS/encrypt": xtsEncrypt,
	"AES-XTS/decrypt": xtsDecrypt,
}

func getConfig(args [][]byte) error {
	if len(args) != 0 {
		return fmt.Errorf("getConfig received %d args", len(args))
	}

	return reply([]byte(`[
	{
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
	}
]`))
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
	if n, err := os.Stdout.Write(lengths[:lengthsLength]); n != lengthsLength || err != nil {
		return fmt.Errorf("write failed: %s", err)
	}

	for _, response := range responses {
		if n, err := os.Stdout.Write(response); n != len(response) || err != nil {
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

const (
	maxArgs       = 8
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
