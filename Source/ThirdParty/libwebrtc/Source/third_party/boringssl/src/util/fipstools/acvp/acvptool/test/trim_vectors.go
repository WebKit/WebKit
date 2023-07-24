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

//go:build ignore

// trimvectors takes an ACVP vector set file and discards all but a single test
// from each test group. This hope is that this achieves good coverage without
// having to check in megabytes worth of JSON files.
package main

import (
	"encoding/json"
	"os"
)

func main() {
	var vectorSets []any
	decoder := json.NewDecoder(os.Stdin)
	if err := decoder.Decode(&vectorSets); err != nil {
		panic(err)
	}

	// The first element is the metadata which is left unmodified.
	for i := 1; i < len(vectorSets); i++ {
		vectorSet := vectorSets[i].(map[string]any)
		testGroups := vectorSet["testGroups"].([]any)
		for _, testGroupInterface := range testGroups {
			testGroup := testGroupInterface.(map[string]any)
			tests := testGroup["tests"].([]any)

			keepIndex := 10
			if keepIndex >= len(tests) {
				keepIndex = len(tests) - 1
			}

			testGroup["tests"] = []any{tests[keepIndex]}
		}
	}

	encoder := json.NewEncoder(os.Stdout)
	encoder.SetIndent("", "  ")
	if err := encoder.Encode(vectorSets); err != nil {
		panic(err)
	}
}
