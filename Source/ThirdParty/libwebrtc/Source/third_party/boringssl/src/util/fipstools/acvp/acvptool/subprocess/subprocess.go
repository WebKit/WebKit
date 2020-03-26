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
		"SHA-1":         &hashPrimitive{"SHA-1", 20, m},
		"SHA2-224":      &hashPrimitive{"SHA2-224", 28, m},
		"SHA2-256":      &hashPrimitive{"SHA2-256", 32, m},
		"SHA2-384":      &hashPrimitive{"SHA2-384", 48, m},
		"SHA2-512":      &hashPrimitive{"SHA2-512", 64, m},
		"ACVP-AES-ECB":  &blockCipher{"AES", 16, false, m},
		"ACVP-AES-CBC":  &blockCipher{"AES-CBC", 16, true, m},
		"HMAC-SHA-1":    &hmacPrimitive{"HMAC-SHA-1", 20, m},
		"HMAC-SHA2-224": &hmacPrimitive{"HMAC-SHA2-224", 28, m},
		"HMAC-SHA2-256": &hmacPrimitive{"HMAC-SHA2-256", 32, m},
		"HMAC-SHA2-384": &hmacPrimitive{"HMAC-SHA2-384", 48, m},
		"HMAC-SHA2-512": &hmacPrimitive{"HMAC-SHA2-512", 64, m},
		"ctrDRBG":       &drbg{"ctrDRBG", map[string]bool{"AES-128": true, "AES-192": true, "AES-256": true}, m},
		"hmacDRBG":      &drbg{"hmacDRBG", map[string]bool{"SHA-1": true, "SHA2-224": true, "SHA2-256": true, "SHA2-384": true, "SHA2-512": true}, m},
		"ECDSA":         &ecdsa{"ECDSA", map[string]bool{"P-224": true, "P-256": true, "P-384": true, "P-521": true}, m},
	}

	return m
}

// Close signals the child process to exit and waits for it to complete.
func (m *Subprocess) Close() {
	m.stdout.Close()
	m.stdin.Close()
	m.cmd.Wait()
}

// transact performs a single request--response pair with the subprocess.
func (m *Subprocess) transact(cmd string, expectedResults int, args ...[]byte) ([][]byte, error) {
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
	results, err := m.transact("getConfig", 1)
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
func (m *Subprocess) Process(algorithm string, vectorSet []byte) ([]byte, error) {
	prim, ok := m.primitives[algorithm]
	if !ok {
		return nil, fmt.Errorf("unknown algorithm %q", algorithm)
	}
	ret, err := prim.Process(vectorSet)
	if err != nil {
		return nil, err
	}
	return json.Marshal(ret)
}

type primitive interface {
	Process(vectorSet []byte) (interface{}, error)
}
