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

// ar.go contains functions for parsing .a archive files.

package main

import (
	"bytes"
	"errors"
	"io"
	"strconv"
	"strings"
)

// ParseAR parses an archive file from r and returns a map from filename to
// contents, or else an error.
func ParseAR(r io.Reader) (map[string][]byte, error) {
	// See https://en.wikipedia.org/wiki/Ar_(Unix)#File_format_details
	const expectedMagic = "!<arch>\n"
	var magic [len(expectedMagic)]byte
	if _, err := io.ReadFull(r, magic[:]); err != nil {
		return nil, err
	}
	if string(magic[:]) != expectedMagic {
		return nil, errors.New("ar: not an archive file")
	}

	const filenameTableName = "//"
	const symbolTableName = "/"
	var longFilenameTable []byte
	ret := make(map[string][]byte)

	for {
		var header [60]byte
		if _, err := io.ReadFull(r, header[:]); err != nil {
			if err == io.EOF {
				break
			}
			return nil, errors.New("ar: error reading file header: " + err.Error())
		}

		name := strings.TrimRight(string(header[:16]), " ")
		sizeStr := strings.TrimRight(string(header[48:58]), "\x00 ")
		size, err := strconv.ParseUint(sizeStr, 10, 64)
		if err != nil {
			return nil, errors.New("ar: failed to parse file size: " + err.Error())
		}

		// File contents are padded to a multiple of two bytes
		storedSize := size
		if storedSize%2 == 1 {
			storedSize++
		}

		contents := make([]byte, storedSize)
		if _, err := io.ReadFull(r, contents); err != nil {
			return nil, errors.New("ar: error reading file contents: " + err.Error())
		}
		contents = contents[:size]

		switch {
		case name == filenameTableName:
			if longFilenameTable != nil {
				return nil, errors.New("ar: two filename tables found")
			}
			longFilenameTable = contents
			continue

		case name == symbolTableName:
			continue

		case len(name) > 1 && name[0] == '/':
			if longFilenameTable == nil {
				return nil, errors.New("ar: long filename reference found before filename table")
			}

			// A long filename is stored as "/" followed by a
			// base-10 offset in the filename table.
			offset, err := strconv.ParseUint(name[1:], 10, 64)
			if err != nil {
				return nil, errors.New("ar: failed to parse filename offset: " + err.Error())
			}
			if offset > uint64((^uint(0))>>1) {
				return nil, errors.New("ar: filename offset overflow")
			}

			if int(offset) > len(longFilenameTable) {
				return nil, errors.New("ar: filename offset out of bounds")
			}

			filename := longFilenameTable[offset:]
			if i := bytes.IndexByte(filename, '/'); i < 0 {
				return nil, errors.New("ar: unterminated filename in table")
			} else {
				filename = filename[:i]
			}

			name = string(filename)

		default:
			name = strings.TrimRight(name, "/")
		}

		ret[name] = contents
	}

	return ret, nil
}
