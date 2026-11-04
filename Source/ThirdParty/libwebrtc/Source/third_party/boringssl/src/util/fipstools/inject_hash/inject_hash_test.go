// Copyright 2026 The BoringSSL Authors
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

package main

import (
	"bytes"
	_ "embed"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"boringssl.googlesource.com/boringssl.git/util/fipstools/fipscommon"
)

//go:embed testdata/test.o
var testObj []byte

//go:embed testdata/empty.o
var emptyObj []byte

func TestInjectHashTextAndROData(t *testing.T) {
	tmpDir := t.TempDir()
	inPath := filepath.Join(tmpDir, "input.o")
	outPath := filepath.Join(tmpDir, "output.o")

	if err := os.WriteFile(inPath, testObj, 0644); err != nil {
		t.Fatalf("failed to write input file: %s", err)
	}

	if err := do(outPath, inPath, "", "", false); err != nil {
		t.Fatalf("do() failed: %s", err)
	}

	outBytes, err := os.ReadFile(outPath)
	if err != nil {
		t.Fatalf("failed to read output file: %s", err)
	}

	if len(outBytes) != len(testObj) {
		t.Fatalf("output length %d does not match input length %d", len(outBytes), len(testObj))
	}

	hashOffset := bytes.Index(testObj, fipscommon.UninitHashValue[:])
	if hashOffset < 0 {
		t.Fatalf("could not find uninit hash value in testObj")
	}

	// Verify all surrounding bytes before the hash are strictly unchanged.
	if !bytes.Equal(outBytes[:hashOffset], testObj[:hashOffset]) {
		t.Errorf("bytes before the hash value were modified")
	}

	// Verify all surrounding bytes after the hash are strictly unchanged.
	if !bytes.Equal(outBytes[hashOffset+32:], testObj[hashOffset+32:]) {
		t.Errorf("bytes after the hash value were modified")
	}

	// Verify the placeholder was overwritten with the expected hash.
	const wantHash = "28369e972bef5e55f32607c72f3434d94b621e141f32f4d196c73183b15c9297"
	gotHash := fmt.Sprintf("%x", outBytes[hashOffset:hashOffset+32])
	if gotHash != wantHash {
		t.Errorf("injected hash mismatch:\n  got:  %s\n  want: %s", gotHash, wantHash)
	}
}

func TestIgnoreMissing(t *testing.T) {
	noHashObj := bytes.ReplaceAll(testObj, fipscommon.UninitHashValue[:], make([]byte, 32))

	tests := []struct {
		name       string
		inputBytes []byte
		wantErr    string
	}{
		{
			name:       "MissingSymbols",
			inputBytes: emptyObj,
			wantErr:    "could not find .text module boundaries",
		},
		{
			name:       "MissingUninitHash",
			inputBytes: noHashObj,
			wantErr:    "did not find uninitialised hash value",
		},
		{
			name:       "NonELFInput",
			inputBytes: []byte("THIS_IS_A_TEXT_FILE_NOT_AN_ELF_OBJECT"),
			wantErr:    "failed to parse object",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			tmpDir := t.TempDir()
			inPath := filepath.Join(tmpDir, "input.o")
			outPath := filepath.Join(tmpDir, "output.o")

			if err := os.WriteFile(inPath, tc.inputBytes, 0644); err != nil {
				t.Fatalf("failed to write input file: %s", err)
			}

			// Without ignoreMissing: must fail with expected error
			err := do(outPath, inPath, "", "", false)
			if err == nil || !strings.Contains(err.Error(), tc.wantErr) {
				t.Errorf("expected error containing %q, got: %v", tc.wantErr, err)
			}

			// With ignoreMissing: must succeed and keep output byte-identical to input
			if err := do(outPath, inPath, "", "", true); err != nil {
				t.Fatalf("do(ignoreMissing=true) failed: %s", err)
			}
			outBytes, err := os.ReadFile(outPath)
			if err != nil {
				t.Fatalf("failed to read output file: %s", err)
			}
			if !bytes.Equal(outBytes, tc.inputBytes) {
				t.Errorf("output was modified when ignoring missing symbols/hashes")
			}
		})
	}
}
