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
	"bufio"
	"bytes"
	"fmt"
	"os"
	"slices"
	"strings"
)

// addedAsmSymbols are additional symbols to include in prefixing,
// even if not found by scanning the asm files.
var addedAsmSymbols = []string{
	"p_thread_callback_boringssl",
}

// CollectAsmGlobals collects assembly global symbols, deduplicated and sorted.
// Inputs are paths to both original and fully templated assembly source files,
// including GAS assembly source .S and NASM .asm files.
// It will understand symbols prefixed with double underscores as private,
// symbols prefixed with a *single* underscore as public on Apple platforms.
func CollectAsmGlobals(srcs []string) ([]string, error) {
	syms := make(map[string]struct{})
	for _, sym := range addedAsmSymbols {
		syms[sym] = struct{}{}
	}
	for _, src := range srcs {
		var file *os.File
		file, err := os.Open(src)
		if err != nil {
			return nil, err
		}
		defer file.Close()
		scanner := bufio.NewScanner(file)
		for scanner.Scan() {
			line := scanner.Bytes()
			tokens := bytes.Fields(line)
			if len(tokens) < 2 {
				continue
			}
			directive := strings.ToLower(string(tokens[0]))
			sym := string(tokens[1])
			switch directive {
			case ".global", "global", ".globl", ".extern", "extern":
				if strings.HasPrefix(sym, "__") {
					continue
				}
				sym := strings.TrimPrefix(sym, "_")
				if _, exists := syms[sym]; !exists {
					syms[sym] = struct{}{}
				}
			}
		}
	}
	var ret []string
	for sym := range syms {
		ret = append(ret, sym)
	}
	slices.Sort(ret)
	return ret, nil
}

// BuildAsmGlobalsCHeader builds a symbol prefixing include for C.
func BuildAsmGlobalsCHeader(syms []string) []byte {
	var output bytes.Buffer
	writeHeader(&output, "//")
	output.WriteString(`
// IWYU pragma: private

#ifndef OPENSSL_HEADER_PREFIX_SYMBOLS_INTERNAL_C_H
#define OPENSSL_HEADER_PREFIX_SYMBOLS_INTERNAL_C_H

#include <openssl/prefix_symbols.h>


#if defined(BORINGSSL_PREFIX)

`)
	// Not using redefine_extname here, as some asm symbols are conditionally inline functions
	// (on platforms with no asm implementation).
	for _, sym := range syms {
		fmt.Fprintf(&output, "#define %s BORINGSSL_ADD_PREFIX(%s)\n", sym, sym)
	}
	output.WriteString(`
#endif  // BORINGSSL_PREFIX

#endif  // OPENSSL_HEADER_PREFIX_SYMBOLS_INTERNAL_C_H
`)
	return output.Bytes()
}

// BuildAsmGlobalsGasHeader builds a symbol prefixing include for the GNU Assembler (gas).
func BuildAsmGlobalsGasHeader(syms []string) []byte {
	var output bytes.Buffer
	writeHeader(&output, "//")
	output.WriteString(`
#ifndef OPENSSL_HEADER_PREFIX_SYMBOLS_INTERNAL_S_H
#define OPENSSL_HEADER_PREFIX_SYMBOLS_INTERNAL_S_H

#include <openssl/prefix_symbols.h>


#if defined(BORINGSSL_PREFIX)

`)
	output.WriteString("#if defined(__APPLE__)\n")
	output.WriteString("\n")
	for _, sym := range syms {
		fmt.Fprintf(&output, "#define _%s BORINGSSL_ADD_USER_LABEL_AND_PREFIX(%s)\n", sym, sym)
	}
	output.WriteString("\n")
	output.WriteString("#else  // __APPLE__\n")
	output.WriteString("\n")
	for _, sym := range syms {
		fmt.Fprintf(&output, "#define %s BORINGSSL_ADD_USER_LABEL_AND_PREFIX(%s)\n", sym, sym)
	}
	output.WriteString("\n")
	output.WriteString("#endif  // __APPLE__\n")
	output.WriteString(`
#endif  // BORINGSSL_PREFIX

#endif  // OPENSSL_HEADER_PREFIX_SYMBOLS_INTERNAL_S_H
`)
	return output.Bytes()
}

// BuildAsmGlobalsNasmX86Header builds a symbol prefixing include for the Netwide Assembler (nasm).
func BuildAsmGlobalsNasmX86Header(syms []string) []byte {
	var output bytes.Buffer
	writeHeader(&output, ";")
	output.WriteString(`
%ifndef OPENSSL_HEADER_GEN_BORINGSSL_PREFIX_SYMBOLS_INTERNAL_X86_WIN_ASM_H
%define OPENSSL_HEADER_GEN_BORINGSSL_PREFIX_SYMBOLS_INTERNAL_X86_WIN_ASM_H


%ifdef BORINGSSL_PREFIX

`)
	for _, sym := range syms {
		fmt.Fprintf(&output, "%%define _%s _ %%+ BORINGSSL_PREFIX %%+ _%s\n", sym, sym)
	}
	output.WriteString(`
%endif  ; BORINGSSL_PREFIX

%endif  ; OPENSSL_HEADER_GEN_BORINGSSL_PREFIX_SYMBOLS_INTERNAL_X86_WIN_ASM_H
`)
	return output.Bytes()
}

// BuildAsmGlobalsNasmX8664Header builds a symbol prefixing include for the Netwide Assembler (nasm).
func BuildAsmGlobalsNasmX8664Header(syms []string) []byte {
	var output bytes.Buffer
	writeHeader(&output, ";")
	output.WriteString(`
%ifndef OPENSSL_HEADER_GEN_BORINGSSL_PREFIX_SYMBOLS_INTERNAL_X86_64_WIN_ASM_H
%define OPENSSL_HEADER_GEN_BORINGSSL_PREFIX_SYMBOLS_INTERNAL_X86_64_WIN_ASM_H


%ifdef BORINGSSL_PREFIX

`)
	for _, sym := range syms {
		fmt.Fprintf(&output, "%%define %s BORINGSSL_PREFIX %%+ _%s\n", sym, sym)
	}
	output.WriteString(`
%endif  ; BORINGSSL_PREFIX

%endif  ; OPENSSL_HEADER_GEN_BORINGSSL_PREFIX_SYMBOLS_INTERNAL_X86_64_WIN_ASM_H
`)
	return output.Bytes()
}
