// Copyright (c) 2025 The BoringSSL Authors
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

//go:build ignore

// extract_identifiers_clang_json parses the BoringSSL public includes and (for now)
// outputs a report of all identifiers defined therein. Sample usage:
//
// for f in include/openssl/*.h; do echo "#include <${f#include/}>"; done |\
//   clang++ -x c++ -std=c++17 -Iinclude -fsyntax-only -Xclang -ast-dump=json - \
//   go run util/extract_identifiers_clang_json.go > extract_identifiers.txt
//
// Note that right now the output of this tool is for human use only.
// The tool will likely be changed further for the purpose of symbol prefixing
// and auditing thereof.

package main

import (
	"flag"
	"fmt"
	"log"
	"os"

	"boringssl.googlesource.com/boringssl.git/util/idextractor"
)

var (
	dumpTree          = flag.Bool("dump_tree", false, "dump syntax tree while processing")
	dumpFullTree      = flag.Bool("dump_full_tree", false, "dump syntax tree while processing including system headers")
	keepGoing         = flag.Bool("keep_going", false, "continue even after errors")
	language          = flag.String("language", "C", "language to consider the source to be")
	globalSymbolsOnly = flag.Bool("global_symbols_only", false, "only output a list of names that may become (part of) linker symbols and are not namespaced")
)

func reportIdentifierGlobalSymbolsOnly(id idextractor.IdentifierInfo) error {
	// NOTE: This is over-exporting.
	// At least for quite a while we can't namespace the
	// enum, struct and union tags either
	// as they may be forward declared by callers.
	// The idea is to first get this overzealous namespacing to work,
	// and then to remove those forward declaration prone symbols
	// at least initially (and possibly namespace them then one by one).
	if id.Symbol == id.Identifier && id.Linkage != "static" && id.Tag != "typedef" && id.Tag != "using" {
		fmt.Printf("%s\n", id.Identifier)
	}
	return nil
}

var seen = map[string]string{}

func reportIdentifierVerbose(id idextractor.IdentifierInfo) error {
	linkage := ""
	if id.Linkage != "" {
		linkage = id.Linkage + " "
	}
	declaration := fmt.Sprintf("%s%s %s;", linkage, id.Tag, id.Symbol)
	key := id.Symbol
	previous, found := seen[key]
	if found {
		if previous != declaration {
			return fmt.Errorf("duplicate distinct definition of %v: %v and %v", key, previous, declaration)
		}
		return nil
	}
	seen[key] = declaration

	// Append some debug info.
	// This might be used later to generate symbol renaming headers,
	// but is generally useful to humans debugging this tool's output.
	var suffix string
	if id.File != "" {
		suffix += fmt.Sprintf(" %s:%v (%v-%v)", id.File, id.Line, id.Start, id.End)
	}
	for _, arg := range id.FunctionArgs {
		if arg == "" {
			arg = "_"
		}
		suffix += " " + arg
	}
	if suffix != "" {
		suffix = "  //" + suffix
	}
	fmt.Printf("%s%s\n", declaration, suffix)
	return nil
}

// Main is the main program.
func Main() error {
	report := reportIdentifierVerbose
	if *globalSymbolsOnly {
		report = reportIdentifierGlobalSymbolsOnly
	}
	x := idextractor.New(report, idextractor.Options{
		DumpTree:     *dumpTree,
		DumpFullTree: *dumpFullTree,
		KeepGoing:    *keepGoing,
		Language:     *language,
	})
	return x.Parse(os.Stdin)
}

// main runs Main turning errors into exit codes.
func main() {
	flag.Parse()
	err := Main()
	if err != nil {
		log.Panicf("error returned from Main: %v", err)
	}
}
