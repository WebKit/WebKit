// Copyright (c) 2026 The BoringSSL Authors
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

// Public API for Clang based identifier extraction.

package idextractor

import (
	"encoding/json"
	"io"
)

// Options are the options for tree walking.
type Options struct {
	// DumpTree prints the tree as it is parsed, but only for BoringSSL code.
	DumpTree bool
	// DumpFullTree prints the tree as it is parsed, even for system headers.
	DumpFullTree bool
	// KeepGoing does not bail out on parse errors.
	KeepGoing bool
	// Language is the langauge to parse the AST as.
	Language string
}

// New creates a new identiifer extractor.
func New(reporter func(IdentifierInfo) error, options Options) *extractor {
	x := &extractor{
		extractorStatic: &extractorStatic{
			reportIdentifier: reporter,
			options:          options,
		},
		language: options.Language,
	}
	return x
}

// Parse parses the Clang AST from the given reader.
// It will invoke the reporter function passed to New
// for each declared identifier encountered.
func (x extractor) Parse(r io.Reader) error {
	j := json.NewDecoder(r)
	for j.More() {
		var root node
		err := j.Decode(&root)
		if err != nil {
			return err
		}
		root.decompressLocs()
		err = x.visit(&root)
		if err != nil {
			return err
		}
	}
	return nil
}
