// Copyright (c) 2019, Google Inc.
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

// Package subprocess contains functionality to talk to a modulewrapper for
// testing of various algorithm implementations.
package subprocess

import (
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
)

// Transactable provides an interface to allow test injection of transactions
// that don't call a server.
type Transactable interface {
	Transact(cmd string, expectedResults int, args ...[]byte) ([][]byte, error)
}

// Subprocess is a "middle" layer that interacts with a FIPS module via running
// a command and speaking a simple protocol over stdin/stdout.
type Subprocess struct {
	cmd        *exec.Cmd
	stdin      io.WriteCloser
	stdout     io.ReadCloser
	primitives map[string]primitive
}

// New returns a new Subprocess middle layer that runs the given binary.
func New(path string) (*Subprocess, error) {
	cmd := exec.Command(path)
	cmd.Stderr = os.Stderr
	stdin, err := cmd.StdinPipe()
	if err != nil {
		return nil, err
	}
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return nil, err
	}

	if err := cmd.Start(); err != nil {
		return nil, err
	}

	return NewWithIO(cmd, stdin, stdout), nil
}

// NewWithIO returns a new Subprocess middle layer with the given ReadCloser and
// WriteCloser. The returned Subprocess will call Wait on the Cmd when closed.
func NewWithIO(cmd *exec.Cmd, in io.WriteCloser, out io.ReadCloser) *Subprocess {
	m := &Subprocess{
		cmd:    cmd,
		stdin:  in,
		stdout: out,
	}

	m.primitives = map[string]primitive{
		"SHA-1":          &hashPrimitive{"SHA-1", 20},
		"SHA2-224":       &hashPrimitive{"SHA2-224", 28},
		"SHA2-256":       &hashPrimitive{"SHA2-256", 32},
		"SHA2-384":       &hashPrimitive{"SHA2-384", 48},
		"SHA2-512":       &hashPrimitive{"SHA2-512", 64},
		"SHA2-512/256":   &hashPrimitive{"SHA2-512/256", 32},
		"ACVP-AES-ECB":   &blockCipher{"AES", 16, 2, true, false, iterateAES},
		"ACVP-AES-CBC":   &blockCipher{"AES-CBC", 16, 2, true, true, iterateAESCBC},
		"ACVP-AES-CTR":   &blockCipher{"AES-CTR", 16, 1, false, true, nil},
		"ACVP-AES-XTS":   &xts{},
		"ACVP-TDES-ECB":  &blockCipher{"3DES-ECB", 8, 3, true, false, iterate3DES},
		"ACVP-TDES-CBC":  &blockCipher{"3DES-CBC", 8, 3, true, true, iterate3DESCBC},
		"ACVP-AES-GCM":   &aead{"AES-GCM", false},
		"ACVP-AES-GMAC":  &aead{"AES-GCM", false},
		"ACVP-AES-CCM":   &aead{"AES-CCM", true},
		"ACVP-AES-KW":    &aead{"AES-KW", false},
		"ACVP-AES-KWP":   &aead{"AES-KWP", false},
		"HMAC-SHA-1":     &hmacPrimitive{"HMAC-SHA-1", 20},
		"HMAC-SHA2-224":  &hmacPrimitive{"HMAC-SHA2-224", 28},
		"HMAC-SHA2-256":  &hmacPrimitive{"HMAC-SHA2-256", 32},
		"HMAC-SHA2-384":  &hmacPrimitive{"HMAC-SHA2-384", 48},
		"HMAC-SHA2-512":  &hmacPrimitive{"HMAC-SHA2-512", 64},
		"ctrDRBG":        &drbg{"ctrDRBG", map[string]bool{"AES-128": true, "AES-192": true, "AES-256": true}},
		"hmacDRBG":       &drbg{"hmacDRBG", map[string]bool{"SHA-1": true, "SHA2-224": true, "SHA2-256": true, "SHA2-384": true, "SHA2-512": true}},
		"KDF":            &kdfPrimitive{},
		"CMAC-AES":       &keyedMACPrimitive{"CMAC-AES"},
		"RSA":            &rsa{},
		"kdf-components": &tlsKDF{},
		"KAS-ECC-SSC":    &kas{},
		"KAS-FFC-SSC":    &kasDH{},
	}
	m.primitives["ECDSA"] = &ecdsa{"ECDSA", map[string]bool{"P-224": true, "P-256": true, "P-384": true, "P-521": true}, m.primitives}

	return m
}

// Close signals the child process to exit and waits for it to complete.
func (m *Subprocess) Close() {
	m.stdout.Close()
	m.stdin.Close()
	m.cmd.Wait()
}

// Transact performs a single request--response pair with the subprocess.
func (m *Subprocess) Transact(cmd string, expectedResults int, args ...[]byte) ([][]byte, error) {
	argLength := len(cmd)
	for _, arg := range args {
		argLength += len(arg)
	}

	buf := make([]byte, 4*(2+len(args)), 4*(2+len(args))+argLength)
	binary.LittleEndian.PutUint32(buf, uint32(1+len(args)))
	binary.LittleEndian.PutUint32(buf[4:], uint32(len(cmd)))
	for i, arg := range args {
		binary.LittleEndian.PutUint32(buf[4*(i+2):], uint32(len(arg)))
	}
	buf = append(buf, []byte(cmd)...)
	for _, arg := range args {
		buf = append(buf, arg...)
	}

	if _, err := m.stdin.Write(buf); err != nil {
		return nil, err
	}

	buf = buf[:4]
	if _, err := io.ReadFull(m.stdout, buf); err != nil {
		return nil, err
	}

	numResults := binary.LittleEndian.Uint32(buf)
	if int(numResults) != expectedResults {
		return nil, fmt.Errorf("expected %d results from %q but got %d", expectedResults, cmd, numResults)
	}

	buf = make([]byte, 4*numResults)
	if _, err := io.ReadFull(m.stdout, buf); err != nil {
		return nil, err
	}

	var resultsLength uint64
	for i := uint32(0); i < numResults; i++ {
		resultsLength += uint64(binary.LittleEndian.Uint32(buf[4*i:]))
	}

	if resultsLength > (1 << 30) {
		return nil, fmt.Errorf("results too large (%d bytes)", resultsLength)
	}

	results := make([]byte, resultsLength)
	if _, err := io.ReadFull(m.stdout, results); err != nil {
		return nil, err
	}

	ret := make([][]byte, 0, numResults)
	var offset int
	for i := uint32(0); i < numResults; i++ {
		length := binary.LittleEndian.Uint32(buf[4*i:])
		ret = append(ret, results[offset:offset+int(length)])
		offset += int(length)
	}

	return ret, nil
}

// Config returns a JSON blob that describes the supported primitives. The
// format of the blob is defined by ACVP. See
// http://usnistgov.github.io/ACVP/artifacts/draft-fussell-acvp-spec-00.html#rfc.section.11.15.2.1
func (m *Subprocess) Config() ([]byte, error) {
	results, err := m.Transact("getConfig", 1)
	if err != nil {
		return nil, err
	}
	var config []struct {
		Algorithm string `json:"algorithm"`
	}
	if err := json.Unmarshal(results[0], &config); err != nil {
		return nil, errors.New("failed to parse config response from wrapper: " + err.Error())
	}
	for _, algo := range config {
		if _, ok := m.primitives[algo.Algorithm]; !ok {
			return nil, fmt.Errorf("wrapper config advertises support for unknown algorithm %q", algo.Algorithm)
		}
	}
	return results[0], nil
}

// Process runs a set of test vectors and returns the result.
func (m *Subprocess) Process(algorithm string, vectorSet []byte) (interface{}, error) {
	prim, ok := m.primitives[algorithm]
	if !ok {
		return nil, fmt.Errorf("unknown algorithm %q", algorithm)
	}
	ret, err := prim.Process(vectorSet, m)
	if err != nil {
		return nil, err
	}
	return ret, nil
}

type primitive interface {
	Process(vectorSet []byte, t Transactable) (interface{}, error)
}

func uint32le(n uint32) []byte {
	var ret [4]byte
	binary.LittleEndian.PutUint32(ret[:], n)
	return ret[:]
}
