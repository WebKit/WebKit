// Copyright (c) 2017, Google Inc.
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

// check_imported_libraries.go checks that each of its arguments only imports
// allowed libraries. This is used to avoid accidental dependencies on
// libstdc++.so.
package main

import (
	"debug/elf"
	"fmt"
	"os"
	"path/filepath"
)

func checkImportedLibraries(path string) bool {
	file, err := elf.Open(path)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error opening %s: %s\n", path, err)
		return false
	}
	defer file.Close()

	libs, err := file.ImportedLibraries()
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error reading %s: %s\n", path, err)
		return false
	}

	allowCpp := filepath.Base(path) == "libssl.so"
	for _, lib := range libs {
		if lib == "libc.so.6" || lib == "libcrypto.so" || lib == "libpthread.so.0" || lib == "libgcc_s.so.1" {
			continue
		}
		if allowCpp && lib == "libstdc++.so.6" {
			continue
		}
		fmt.Printf("Invalid dependency for %s: %s\n", path, lib)
		fmt.Printf("All dependencies:\n")
		for _, lib := range libs {
			fmt.Printf("    %s\n", lib)
		}
		return false
	}
	return true
}

func main() {
	ok := true
	for _, path := range os.Args[1:] {
		if !checkImportedLibraries(path) {
			ok = false
		}
	}
	if !ok {
		os.Exit(1)
	}
}
