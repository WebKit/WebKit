// Copyright (c) 2024, Google Inc.
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

//go:build ignore

// This trivial program is used to corrupt the FIPS module. This is done as
// part of FIPS testing to show that the integrity check is effective.
//
// It finds the (sole) occurance of a given hex pattern in a file and flips the
// first bit. The hex pattern is intended to be the output of running
// `BORINGSSL_FIPS_SHOW_HASH=1 ninja bcm.o`, i.e. the integrity hash value of
// the module. By flipping the first bit we ensure that the check will
// mismatch.
//
// This is a simplier version of `break-hash.go` for when we're building with
// BORINGSSL_FIPS_SHOW_HASH. (But we don't do that in all cases.)

package main

import (
	"bytes"
	"encoding/hex"
	"fmt"
	"io/ioutil"
	"os"
)

func main() {
	if len(os.Args) != 3 {
		fmt.Fprintln(os.Stderr, "Usage: program <hex_string> <file_path>")
		os.Exit(1)
	}

	hexString := os.Args[1]
	filePath := os.Args[2]

	// Decode hex string
	searchBytes, err := hex.DecodeString(hexString)
	if err != nil {
		fmt.Fprintln(os.Stderr, "Error decoding hex string:", err)
		os.Exit(1)
	}

	// Read file contents
	content, err := ioutil.ReadFile(filePath)
	if err != nil {
		fmt.Fprintln(os.Stderr, "Error reading file:", err)
		os.Exit(1)
	}

	// Search for the occurrence of the hex string
	index := bytes.Index(content, searchBytes)
	if index == -1 {
		fmt.Fprintln(os.Stderr, "Hex string not found in the file")
		os.Exit(1)
	}

	// Check for other occurrences
	if bytes.Index(content[index+len(searchBytes):], searchBytes) != -1 {
		fmt.Fprintln(os.Stderr, "Multiple occurrences of the hex string found")
		os.Exit(1)
	}

	// Flip the first bit
	content[index] ^= 0x80

	// Write updated contents to stdout
	os.Stdout.Write(content)
}
