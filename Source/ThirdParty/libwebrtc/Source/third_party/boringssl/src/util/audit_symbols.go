// Copyright 2018 The BoringSSL Authors
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

// read_symbols scans one or more .a files and, for each object contained in
// the .a files, reads the list of symbols in that object file.
package main

import (
	"bytes"
	"debug/elf"
	"debug/macho"
	"debug/pe"
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"maps"
	"os"
	"regexp"
	"runtime"
	"slices"
	"strings"

	"boringssl.googlesource.com/boringssl.git/util/ar"
)

// skipWeakSymbols is the list of regular expressions marking symbols to skip.
//
// Inline functions, etc., from the compiler or language runtime will naturally
// end up in the library, to be deduplicated against other object files. Such
// symbols should not be prefixed. It is a limitation of this symbol-prefixing
// strategy that we cannot distinguish our own inline symbols (which should be
// prefixed) from the system's (which should not), so we skip known system
// symbols.
var skipWeakSymbols = []*regexp.Regexp{
	// Symbols on Linux and other platforms with IA-64 name mangling.
	regexp.MustCompile(`^DW\.ref\.__gxx_personality_.*`), // libstdc++ exception handling
	regexp.MustCompile(`^_Z6memchr.*`),                   // memchr()
	regexp.MustCompile(`^_Z6strchr.*`),                   // strchr()
	regexp.MustCompile(`^_ZN9__gnu_cxx.*`),               // __gnu_cxx::
	regexp.MustCompile(`^_ZTI.*`),                        // typeinfo
	regexp.MustCompile(`^_ZTS.*`),                        // typeinfo name
	regexp.MustCompile(`^_ZTV.*`),                        // vtable
	regexp.MustCompile(`^_Z[A-Z]*St.*`),                  // std::
	regexp.MustCompile(`^_Zd.*`),                         // operator delete()
	regexp.MustCompile(`^_Zn.*`),                         // operator new()
	regexp.MustCompile(`^__\w+\.get_pc_thunk\..*`),       // PIC
	regexp.MustCompile(`^___asan_.*`),                    // AddressSanitizer
	regexp.MustCompile(`^__cxa_.*`),                      // libc++abi
	regexp.MustCompile(`^__dynamic_cast$`),               // libc++abi
	regexp.MustCompile(`^__emutls_get_address$`),         // emulated TLS
	regexp.MustCompile(`^__g\w+_personality_.*`),         // unwinding
	regexp.MustCompile(`^__llvm_fs_discriminator__$`),    // FS-AutoFDO
	regexp.MustCompile(`^__msan_.*`),                     // MemorySanitizer

	// Symbols on Windows.
	regexp.MustCompile(`.*<lambda.*`),                                   // Lambda classes
	regexp.MustCompile(`.*@std(@.*)?$`),                                 // std::
	regexp.MustCompile(`.*@stdext(@.*)?$`),                              // stdext::
	regexp.MustCompile(`^(.*\?\?)?__local_stdio_printf_options(@.*)?$`), // stdio
	regexp.MustCompile(`^(.*\?\?)?gai_strerrorA(@.*)?$`),                // gai_strerrorA()
	regexp.MustCompile(`^RtlSecureZeroMemory$`),                         // RtlSecureZeroMemory()
	regexp.MustCompile(`^\?\?2.*`),                                      // operator new()
	regexp.MustCompile(`^\?\?3.*`),                                      // operator delete()
	regexp.MustCompile(`^\?\?_C\@_.*`),                                  // String literals
	regexp.MustCompile(`^\?memchr(@.*)?$`),                              // memchr()
	regexp.MustCompile(`^\?strchr(@.*)?$`),                              // strchr()
	regexp.MustCompile(`^_Check_memory_order$`),                         // std::atomic
	regexp.MustCompile(`^__isa_available_default$`),                     // SSE
	regexp.MustCompile(`^__xmm@.*`),                                     // SSE
	regexp.MustCompile(`^_vfprintf_l$`),                                 // vfprintf()
	regexp.MustCompile(`^_xmm$`),                                        // SSE
	regexp.MustCompile(`^fprintf$`),                                     // fprintf()
	regexp.MustCompile(`^snprintf$`),                                    // snprintf()
	regexp.MustCompile(`^vsnprintf$`),                                   // vsnprintf()
	regexp.MustCompile(`^\?\?_R[0-4].*$`),                               // RTTI
	regexp.MustCompile(`^_Avx2WmemEnabledWeakValue$`),                   // MSVC 14.50+ CRT
	regexp.MustCompile(`^time$`),                                        // MSVC 14.50+ CRT

	// Symbols in the FIPS module.
	// They are provided for tooling only and should not be read internally.
	regexp.MustCompile(`^BORINGSSL_bcm_(rodata|text)_(start|end)$`),
}

var skipSymbols = []*regexp.Regexp{}

const (
	ObjFileFormatELF   = "elf"
	ObjFileFormatMachO = "macho"
	ObjFileFormatPE    = "pe"
)

var (
	outFlag           = flag.String("out", "-", "File to write output symbols")
	objFileFormat     = flag.String("obj-file-format", defaultObjFileFormat(runtime.GOOS), "Object file format to expect (options are elf, macho, pe)")
	ignoreSymbolsWith = flag.String("ignore-symbols-with", "", "symbol infix to ignore (should be the BORINGSSL_PREFIX of the build)")
)

func defaultObjFileFormat(goos string) string {
	switch goos {
	case "linux":
		return ObjFileFormatELF
	case "darwin":
		return ObjFileFormatMachO
	case "windows":
		return ObjFileFormatPE
	default:
		// By returning a value here rather than panicking, the user can still
		// cross-compile from an unsupported platform to a supported platform by
		// overriding this default with a flag. If the user doesn't provide the
		// flag, we will panic during flag parsing.
		return "unsupported"
	}
}

func printAndExit(format string, args ...any) {
	s := fmt.Sprintf(format, args...)
	fmt.Fprintln(os.Stderr, s)
	os.Exit(1)
}

func main() {
	flag.Parse()
	if flag.NArg() < 1 {
		printAndExit("Usage: %s [-out OUT] [-obj-file-format FORMAT] ARCHIVE_FILE [ARCHIVE_FILE [...]]", os.Args[0])
	}
	archiveFiles := flag.Args()

	out := os.Stdout
	if *outFlag != "-" {
		var err error
		out, err = os.Create(*outFlag)
		if err != nil {
			printAndExit("Error opening %q: %s", *outFlag, err)
		}
		defer out.Close()
	}

	// Only add first instance of any symbol; keep track of them in this map.
	symbols := make(map[string]strength)
	collectSymbols := func(name, archive string, contents []byte) {
		syms, err := listSymbols(contents)
		if err != nil {
			printAndExit("Error listing symbols from %q in %q: %s", name, archive, err)
		}
		for s, strength := range syms {
			if _, ok := symbols[s]; !ok {
				symbols[s] = strength
			}
		}
	}

	for _, archive := range archiveFiles {
		f, err := os.Open(archive)
		if err != nil {
			printAndExit("Error opening %s: %s", archive, err)
		}
		objectFiles, err := ar.ParseAR(f)
		f.Close()

		if err != nil {
			if errors.Is(err, ar.NotAnArchiveFile) {
				// Maybe a shared library?
				contents, err := os.ReadFile(archive)
				if err != nil {
					printAndExit("Error reading %s: %s", archive, err)
				}
				collectSymbols(archive, archive, contents)
			} else {
				printAndExit("Error parsing %s: %s", archive, err)
			}
		}

		for name, contents := range objectFiles {
			collectSymbols(name, archive, contents)
		}
	}

	symbolNames := slices.Sorted(maps.Keys(symbols))
	status := 0
SYMBOLS:
	for _, s := range symbolNames {
		if *ignoreSymbolsWith != "" && strings.Contains(s, *ignoreSymbolsWith) {
			continue
		}
		if symbols[s] == weakSymbol {
			for _, symRE := range skipWeakSymbols {
				if symRE.MatchString(s) {
					continue SYMBOLS
				}
			}
		}
		for _, symRE := range skipSymbols {
			if symRE.MatchString(s) {
				continue SYMBOLS
			}
		}
		msg := s
		if *ignoreSymbolsWith != "" {
			msg = fmt.Sprintf("Found %s symbol without %q: %s", symbols[s], *ignoreSymbolsWith, s)
		}
		if _, err := fmt.Fprintln(out, msg); err != nil {
			printAndExit("Error writing to %s: %s", *outFlag, err)
		}
		status = 1
	}
	os.Exit(status)
}

type strength int

const (
	unknownSymbol strength = iota
	weakSymbol
	strongSymbol
)

func (s strength) String() string {
	switch s {
	case unknownSymbol:
		return "unknown"
	case weakSymbol:
		return "weak"
	case strongSymbol:
		return "strong"
	}
	printAndExit("unsupported symbol strength value: %d", int(s))
	return ""
}

func weakIf(weak bool) strength {
	if weak {
		return weakSymbol
	}
	return strongSymbol
}

// listSymbols lists the exported symbols from an object file.
func listSymbols(contents []byte) (map[string]strength, error) {
	switch *objFileFormat {
	case ObjFileFormatELF:
		return listSymbolsELF(contents)
	case ObjFileFormatMachO:
		return listSymbolsMachO(contents)
	case ObjFileFormatPE:
		return listSymbolsPE(contents)
	default:
		return nil, fmt.Errorf("unsupported object file format %q", *objFileFormat)
	}
}

func listSymbolsELF(contents []byte) (map[string]strength, error) {
	f, err := elf.NewFile(bytes.NewReader(contents))
	if err != nil {
		return nil, err
	}
	syms, err := f.Symbols()
	if err == elf.ErrNoSymbols {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}

	names := map[string]strength{}
	minSection := elf.SHN_UNDEF + 1
	maxSection := elf.SHN_UNDEF + elf.SectionIndex(len(f.Sections)-1)
	for _, sym := range syms {
		if elf.ST_BIND(sym.Info) == elf.STB_LOCAL || sym.Section == elf.SHN_UNDEF {
			// Local or undefined symbols don't matter here.
			continue
		}
		if f.Type == elf.ET_DYN && elf.ST_VISIBILITY(sym.Other) == elf.STV_HIDDEN {
			// Ignore any hidden symbols in shared libraries.
			continue
		}
		names[sym.Name] = weakIf(
			elf.ST_BIND(sym.Info) == elf.STB_WEAK || // explicitly weak
				sym.Section == elf.SHN_COMMON || // in the COMMON section
				(sym.Section >= minSection && sym.Section <= maxSection &&
					f.Sections[sym.Section].Flags&elf.SHF_GROUP != 0) || // in a section group (usually also COMMON-like)
				f.Type == elf.ET_DYN) // in a .so
	}
	return names, nil
}

func listSymbolsMachO(contents []byte) (map[string]strength, error) {
	f, err := macho.NewFile(bytes.NewReader(contents))
	if err != nil {
		return nil, err
	}
	if f.Symtab == nil {
		return nil, nil
	}
	names := map[string]strength{}
	for _, sym := range f.Symtab.Syms {
		// Source: https://opensource.apple.com/source/xnu/xnu-3789.51.2/EXTERNAL_HEADERS/mach-o/nlist.h.auto.html
		const (
			N_PEXT uint8 = 0x10 // Private (visibility hidden) external symbol bit
			N_EXT  uint8 = 0x01 // External symbol bit, set for external symbols

			N_TYPE uint8 = 0x0e // mask for the type bits
			N_UNDF uint8 = 0x0  // undefined, n_sect == NO_SECT

			N_WEAK_DEF uint16 = 0x80 // weak symbol
		)

		if sym.Type&N_EXT == 0 || sym.Type&N_TYPE == N_UNDF {
			// Internal or undefined symbols don't matter here.
			continue
		}
		if f.Type == macho.TypeDylib && sym.Type&N_PEXT != 0 {
			// Ignore any hidden symbols in shared libraries.
			continue
		}
		if len(sym.Name) == 0 || sym.Name[0] != '_' {
			return nil, fmt.Errorf("unexpected symbol without underscore prefix: %q", sym.Name)
		}
		names[sym.Name[1:]] = weakIf(
			sym.Desc&N_WEAK_DEF != 0 || // explicitly weak
				f.Type == macho.TypeDylib) // in a .dylib
	}
	return names, nil
}

func listSymbolsDLL(f *pe.File) (map[string]strength, error) {
	ret := map[string]strength{}

	// All offsets in DLLs are relative virtual addresses.
	// To load data from a relative virtual address,
	// one first needs to identify the section the data is contained in.
	readRVA := func(addr, size uint32) ([]byte, error) {
		for _, section := range f.Sections {
			if addr >= section.VirtualAddress && addr-section.VirtualAddress+size <= section.Size {
				data, err := section.Data()
				if err != nil {
					return nil, fmt.Errorf("could not get section %q data: %w", string(section.Name), err)
				}
				return data[int(addr-section.VirtualAddress):][:int(size)], nil
			}
		}
		return nil, fmt.Errorf("address range %08x len %08x not contained in any section", addr, size)
	}
	readRVASZ := func(addr uint32) (string, error) {
		for _, section := range f.Sections {
			if addr >= section.VirtualAddress && addr-section.VirtualAddress < section.Size {
				data, err := section.Data()
				if err != nil {
					return "", fmt.Errorf("could not get section %q data: %w", string(section.Name), err)
				}
				sz, _, _ := bytes.Cut(data[int(addr-section.VirtualAddress):], []byte{0})
				return string(sz), nil
			}
		}
		return "", fmt.Errorf("address %08x not contained in any section", addr)
	}

	// Locate the export directory.
	var exportDirData *pe.DataDirectory
	switch oh := f.OptionalHeader.(type) {
	case *pe.OptionalHeader32:
		exportDirData = &oh.DataDirectory[pe.IMAGE_DIRECTORY_ENTRY_EXPORT]
	case *pe.OptionalHeader64:
		exportDirData = &oh.DataDirectory[pe.IMAGE_DIRECTORY_ENTRY_EXPORT]
	default:
		return nil, fmt.Errorf("unknown DLL bitness")
	}
	exportDirBytes, err := readRVA(exportDirData.VirtualAddress, exportDirData.Size)
	if err != nil {
		return nil, fmt.Errorf("could not load export directory: %w", err)
	}

	// Find the name pointers table.
	var numNamePointers, namePointersAddr uint32
	if _, err := binary.Decode(exportDirBytes[24:][:4], binary.LittleEndian, &numNamePointers); err != nil {
		return nil, fmt.Errorf("could not get name pointer count: %w", err)
	}
	if _, err := binary.Decode(exportDirBytes[32:][:4], binary.LittleEndian, &namePointersAddr); err != nil {
		return nil, fmt.Errorf("could not get name pointer RVA base: %w", err)
	}
	namePointers, err := readRVA(namePointersAddr, numNamePointers*4)
	if err != nil {
		return nil, fmt.Errorf("could not get name pointer table: %w", err)
	}

	// For each name pointer, find the symbol name.
	for i := uint32(0); i < numNamePointers; i++ {
		var addr uint32
		if _, err := binary.Decode(namePointers[4*int(i):][:4], binary.LittleEndian, &addr); err != nil {
			return nil, fmt.Errorf("could not get name pointer %d address: %w", err)
		}
		name, err := readRVASZ(addr)
		if err != nil {
			return nil, fmt.Errorf("could not get name %d string: %w", err)
		}
		ret[name] = weakSymbol // in a .dll
	}

	return ret, nil
}

func listSymbolsPE(contents []byte) (map[string]strength, error) {
	f, err := pe.NewFile(bytes.NewReader(contents))
	if err != nil {
		return nil, fmt.Errorf("pe.NewFile: %w", err)
	}

	if f.Characteristics&pe.IMAGE_FILE_DLL != 0 {
		return listSymbolsDLL(f)
	}

	ret := map[string]strength{}

	// https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#section-number-values
	minSection := int16(1)
	maxSection := int16(len(f.Sections))

	for _, sym := range f.Symbols {
		const (
			// https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#section-number-values
			IMAGE_SYM_UNDEFINED = 0
			// https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#storage-class
			IMAGE_SYM_CLASS_EXTERNAL = 2
		)

		if sym.StorageClass != IMAGE_SYM_CLASS_EXTERNAL || sym.SectionNumber == IMAGE_SYM_UNDEFINED {
			// Internal or undefined symbols don't matter here.
			continue
		}
		name := sym.Name
		if f.Machine == pe.IMAGE_FILE_MACHINE_I386 {
			// On 32-bit Windows, C symbols are decorated by calling
			// convention.
			// https://msdn.microsoft.com/en-us/library/56h2zst2.aspx#FormatC
			if strings.HasPrefix(name, "_") || strings.HasPrefix(name, "@") {
				// __cdecl, __stdcall, or __fastcall. Remove the prefix and
				// suffix, if present.
				name = name[1:]
				if idx := strings.LastIndex(name, "@"); idx >= 0 {
					name = name[:idx]
				}
			} else if idx := strings.LastIndex(name, "@@"); idx >= 0 {
				// __vectorcall. Remove the suffix.
				name = name[:idx]
			}
		}
		ret[name] = weakIf(
			sym.SectionNumber >= minSection && sym.SectionNumber <= maxSection &&
				f.Sections[sym.SectionNumber-minSection].Characteristics&pe.IMAGE_SCN_LNK_COMDAT != 0)
	}

	return ret, nil
}
