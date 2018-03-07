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
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

// inject-hash parses an archive containing a file object file. It finds a FIPS
// module inside that object, calculates its hash and replaces the default hash
// value in the object with the calculated value.
package main

import (
	"bytes"
	"crypto/hmac"
	"crypto/sha512"
	"debug/elf"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"os"
)

func do(outPath, oInput string, arInput string) error {
	var objectBytes []byte
	if len(arInput) > 0 {
		if len(oInput) > 0 {
			return fmt.Errorf("-in-archive and -in-object are mutually exclusive")
		}

		arFile, err := os.Open(arInput)
		if err != nil {
			return err
		}
		defer arFile.Close()

		ar, err := ParseAR(arFile)
		if err != nil {
			return err
		}

		if len(ar) != 1 {
			return fmt.Errorf("expected one file in archive, but found %d", len(ar))
		}

		for _, contents := range ar {
			objectBytes = contents
		}
	} else if len(oInput) > 0 {
		var err error
		if objectBytes, err = ioutil.ReadFile(oInput); err != nil {
			return err
		}
	} else {
		return fmt.Errorf("exactly one of -in-archive or -in-object is required")
	}

	object, err := elf.NewFile(bytes.NewReader(objectBytes))
	if err != nil {
		return errors.New("failed to parse object: " + err.Error())
	}

	// Find the .text section.

	var textSection *elf.Section
	var textSectionIndex elf.SectionIndex
	for i, section := range object.Sections {
		if section.Name == ".text" {
			textSectionIndex = elf.SectionIndex(i)
			textSection = section
			break
		}
	}

	if textSection == nil {
		return errors.New("failed to find .text section in object")
	}

	// Find the starting and ending symbols for the module.

	var startSeen, endSeen bool
	var start, end uint64

	symbols, err := object.Symbols()
	if err != nil {
		return errors.New("failed to parse symbols: " + err.Error())
	}

	for _, symbol := range symbols {
		if symbol.Section != textSectionIndex {
			continue
		}

		switch symbol.Name {
		case "BORINGSSL_bcm_text_start":
			if startSeen {
				return errors.New("duplicate start symbol found")
			}
			startSeen = true
			start = symbol.Value
		case "BORINGSSL_bcm_text_end":
			if endSeen {
				return errors.New("duplicate end symbol found")
			}
			endSeen = true
			end = symbol.Value
		default:
			continue
		}
	}

	if !startSeen || !endSeen {
		return errors.New("could not find module boundaries in object")
	}

	if max := textSection.Size; start > max || start > end || end > max {
		return fmt.Errorf("invalid module boundaries: start: %x, end: %x, max: %x", start, end, max)
	}

	// Extract the module from the .text section and hash it.

	text := textSection.Open()
	if _, err := text.Seek(int64(start), 0); err != nil {
		return errors.New("failed to seek to module start in .text: " + err.Error())
	}
	moduleText := make([]byte, end-start)
	if _, err := io.ReadFull(text, moduleText); err != nil {
		return errors.New("failed to read .text: " + err.Error())
	}

	var zeroKey [64]byte
	mac := hmac.New(sha512.New, zeroKey[:])
	mac.Write(moduleText)
	calculated := mac.Sum(nil)

	// Replace the default hash value in the object with the calculated
	// value and write it out.

	offset := bytes.Index(objectBytes, uninitHashValue[:])
	if offset < 0 {
		return errors.New("did not find uninitialised hash value in object file")
	}

	if bytes.Index(objectBytes[offset+1:], uninitHashValue[:]) >= 0 {
		return errors.New("found two occurrences of uninitialised hash value in object file")
	}

	copy(objectBytes[offset:], calculated)

	return ioutil.WriteFile(outPath, objectBytes, 0644)
}

func main() {
	arInput := flag.String("in-archive", "", "Path to a .a file")
	oInput := flag.String("in-object", "", "Path to a .o file")
	outPath := flag.String("o", "", "Path to output object")

	flag.Parse()

	if err := do(*outPath, *oInput, *arInput); err != nil {
		fmt.Fprintf(os.Stderr, "%s\n", err)
		os.Exit(1)
	}
}
