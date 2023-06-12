// Copyright (c) 2016, Google Inc.
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

package main

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"sort"
	"strconv"
	"strings"
)

func sanitizeName(in string) string {
	in = strings.Replace(in, "-", "_", -1)
	in = strings.Replace(in, ".", "_", -1)
	in = strings.Replace(in, " ", "_", -1)
	return in
}

type object struct {
	name string
	// shortName and longName are the short and long names, respectively. If
	// one is missing, it takes the value of the other, but the
	// corresponding SN_foo or LN_foo macro is not defined.
	shortName, longName       string
	hasShortName, hasLongName bool
	oid                       []int
	encoded                   []byte
}

type objects struct {
	// byNID is the list of all objects, indexed by nid.
	byNID []object
	// nameToNID is a map from object name to nid.
	nameToNID map[string]int
}

func readNumbers(path string) (nameToNID map[string]int, numNIDs int, err error) {
	in, err := os.Open(path)
	if err != nil {
		return nil, 0, err
	}
	defer in.Close()

	nameToNID = make(map[string]int)
	nidsSeen := make(map[int]struct{})

	// Reserve NID 0 for NID_undef.
	numNIDs = 1
	nameToNID["undef"] = 0
	nidsSeen[0] = struct{}{}

	var lineNo int
	scanner := bufio.NewScanner(in)
	for scanner.Scan() {
		line := scanner.Text()
		lineNo++
		withLine := func(err error) error {
			return fmt.Errorf("%s:%d: %s", path, lineNo, err)
		}

		fields := strings.Fields(line)
		if len(fields) == 0 {
			// Skip blank lines.
			continue
		}

		// Each line is a name and a nid, separated by space.
		if len(fields) != 2 {
			return nil, 0, withLine(errors.New("syntax error"))
		}
		name := fields[0]
		nid, err := strconv.Atoi(fields[1])
		if err != nil {
			return nil, 0, withLine(err)
		}
		if nid < 0 {
			return nil, 0, withLine(errors.New("invalid NID"))
		}

		// NID_undef is implicitly defined.
		if name == "undef" && nid == 0 {
			continue
		}

		// Forbid duplicates.
		if _, ok := nameToNID[name]; ok {
			return nil, 0, withLine(fmt.Errorf("duplicate name %q", name))
		}
		if _, ok := nidsSeen[nid]; ok {
			return nil, 0, withLine(fmt.Errorf("duplicate NID %d", nid))
		}

		nameToNID[name] = nid
		nidsSeen[nid] = struct{}{}

		if nid >= numNIDs {
			numNIDs = nid + 1
		}
	}
	if err := scanner.Err(); err != nil {
		return nil, 0, fmt.Errorf("error reading %s: %s", path, err)
	}

	return nameToNID, numNIDs, nil
}

func parseOID(aliases map[string][]int, in []string) (oid []int, err error) {
	if len(in) == 0 {
		return
	}

	// The first entry may be a reference to a previous alias.
	if alias, ok := aliases[sanitizeName(in[0])]; ok {
		in = in[1:]
		oid = append(oid, alias...)
	}

	for _, c := range in {
		val, err := strconv.Atoi(c)
		if err != nil {
			return nil, err
		}
		if val < 0 {
			return nil, fmt.Errorf("negative component")
		}
		oid = append(oid, val)
	}
	return
}

func appendBase128(dst []byte, value int) []byte {
	// Zero is encoded with one, not zero bytes.
	if value == 0 {
		return append(dst, 0)
	}

	// Count how many bytes are needed.
	var l int
	for n := value; n != 0; n >>= 7 {
		l++
	}
	for ; l > 0; l-- {
		b := byte(value>>uint(7*(l-1))) & 0x7f
		if l > 1 {
			b |= 0x80
		}
		dst = append(dst, b)
	}
	return dst
}

func encodeOID(oid []int) []byte {
	if len(oid) < 2 {
		return nil
	}

	var der []byte
	der = appendBase128(der, 40*oid[0]+oid[1])
	for _, value := range oid[2:] {
		der = appendBase128(der, value)
	}
	return der
}

func readObjects(numPath, objectsPath string) (*objects, error) {
	nameToNID, numNIDs, err := readNumbers(numPath)
	if err != nil {
		return nil, err
	}

	in, err := os.Open(objectsPath)
	if err != nil {
		return nil, err
	}
	defer in.Close()

	// Implicitly define NID_undef.
	objs := &objects{
		byNID:     make([]object, numNIDs),
		nameToNID: make(map[string]int),
	}

	objs.byNID[0] = object{
		name:         "undef",
		shortName:    "UNDEF",
		longName:     "undefined",
		hasShortName: true,
		hasLongName:  true,
	}
	objs.nameToNID["undef"] = 0

	var module, nextName string
	var lineNo int
	longNamesSeen := make(map[string]struct{})
	shortNamesSeen := make(map[string]struct{})
	aliases := make(map[string][]int)
	scanner := bufio.NewScanner(in)
	for scanner.Scan() {
		line := scanner.Text()
		lineNo++
		withLine := func(err error) error {
			return fmt.Errorf("%s:%d: %s", objectsPath, lineNo, err)
		}

		// Remove comments.
		idx := strings.IndexRune(line, '#')
		if idx >= 0 {
			line = line[:idx]
		}

		// Skip empty lines.
		line = strings.TrimSpace(line)
		if len(line) == 0 {
			continue
		}

		if line[0] == '!' {
			args := strings.Fields(line)
			switch args[0] {
			case "!module":
				if len(args) != 2 {
					return nil, withLine(errors.New("too many arguments"))
				}
				module = sanitizeName(args[1]) + "_"
			case "!global":
				module = ""
			case "!Cname":
				// !Cname directives override the name for the
				// next object.
				if len(args) != 2 {
					return nil, withLine(errors.New("too many arguments"))
				}
				nextName = sanitizeName(args[1])
			case "!Alias":
				// !Alias directives define an alias for an OID
				// without emitting an object.
				if len(nextName) != 0 {
					return nil, withLine(errors.New("!Cname directives may not modify !Alias directives."))
				}
				if len(args) < 3 {
					return nil, withLine(errors.New("not enough arguments"))
				}
				aliasName := module + sanitizeName(args[1])
				oid, err := parseOID(aliases, args[2:])
				if err != nil {
					return nil, withLine(err)
				}
				if _, ok := aliases[aliasName]; ok {
					return nil, withLine(fmt.Errorf("duplicate name '%s'", aliasName))
				}
				aliases[aliasName] = oid
			default:
				return nil, withLine(fmt.Errorf("unknown directive '%s'", args[0]))
			}
			continue
		}

		fields := strings.Split(line, ":")
		if len(fields) < 2 || len(fields) > 3 {
			return nil, withLine(errors.New("invalid field count"))
		}

		obj := object{name: nextName}
		nextName = ""

		var err error
		obj.oid, err = parseOID(aliases, strings.Fields(fields[0]))
		if err != nil {
			return nil, withLine(err)
		}
		obj.encoded = encodeOID(obj.oid)

		obj.shortName = strings.TrimSpace(fields[1])
		if len(fields) == 3 {
			obj.longName = strings.TrimSpace(fields[2])
		}

		// Long and short names default to each other if missing.
		if len(obj.shortName) == 0 {
			obj.shortName = obj.longName
		} else {
			obj.hasShortName = true
		}
		if len(obj.longName) == 0 {
			obj.longName = obj.shortName
		} else {
			obj.hasLongName = true
		}
		if len(obj.shortName) == 0 || len(obj.longName) == 0 {
			return nil, withLine(errors.New("object with no name"))
		}

		// If not already specified, prefer the long name if it has no
		// spaces, otherwise the short name.
		if len(obj.name) == 0 && strings.IndexRune(obj.longName, ' ') < 0 {
			obj.name = sanitizeName(obj.longName)
		}
		if len(obj.name) == 0 {
			obj.name = sanitizeName(obj.shortName)
		}
		obj.name = module + obj.name

		// Check for duplicate names.
		if _, ok := aliases[obj.name]; ok {
			return nil, withLine(fmt.Errorf("duplicate name '%s'", obj.name))
		}
		if _, ok := shortNamesSeen[obj.shortName]; ok && len(obj.shortName) > 0 {
			return nil, withLine(fmt.Errorf("duplicate short name '%s'", obj.shortName))
		}
		if _, ok := longNamesSeen[obj.longName]; ok && len(obj.longName) > 0 {
			return nil, withLine(fmt.Errorf("duplicate long name '%s'", obj.longName))
		}

		// Allocate a NID.
		nid, ok := nameToNID[obj.name]
		if !ok {
			nid = len(objs.byNID)
			objs.byNID = append(objs.byNID, object{})
		}

		objs.byNID[nid] = obj
		objs.nameToNID[obj.name] = nid

		longNamesSeen[obj.longName] = struct{}{}
		shortNamesSeen[obj.shortName] = struct{}{}
		aliases[obj.name] = obj.oid
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}

	// The kNIDsIn*Order constants assume each NID fits in a uint16_t.
	if len(objs.byNID) > 0xffff {
		return nil, errors.New("too many NIDs allocated")
	}

	return objs, nil
}

func writeNumbers(path string, objs *objects) error {
	out, err := os.Create(path)
	if err != nil {
		return err
	}
	defer out.Close()

	for nid, obj := range objs.byNID {
		if len(obj.name) == 0 {
			continue
		}
		if _, err := fmt.Fprintf(out, "%s\t\t%d\n", obj.name, nid); err != nil {
			return err
		}
	}
	return nil
}

func clangFormat(input string) (string, error) {
	var b bytes.Buffer
	cmd := exec.Command("clang-format")
	cmd.Stdin = strings.NewReader(input)
	cmd.Stdout = &b
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return "", err
	}
	return b.String(), nil
}

func writeHeader(path string, objs *objects) error {
	var b bytes.Buffer
	fmt.Fprintf(&b, `/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG `+"``"+`AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

/* This file is generated by crypto/obj/objects.go. */

#ifndef OPENSSL_HEADER_NID_H
#define OPENSSL_HEADER_NID_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* The nid library provides numbered values for ASN.1 object identifiers and
 * other symbols. These values are used by other libraries to identify
 * cryptographic primitives.
 *
 * A separate objects library, obj.h, provides functions for converting between
 * nids and object identifiers. However it depends on large internal tables with
 * the encodings of every nid defined. Consumers concerned with binary size
 * should instead embed the encodings of the few consumed OIDs and compare
 * against those.
 *
 * These values should not be used outside of a single process; they are not
 * stable identifiers. */


`)

	for nid, obj := range objs.byNID {
		if len(obj.name) == 0 {
			continue
		}

		if obj.hasShortName {
			fmt.Fprintf(&b, "#define SN_%s \"%s\"\n", obj.name, obj.shortName)
		}
		if obj.hasLongName {
			fmt.Fprintf(&b, "#define LN_%s \"%s\"\n", obj.name, obj.longName)
		}
		fmt.Fprintf(&b, "#define NID_%s %d\n", obj.name, nid)

		// Although NID_undef does not have an OID, OpenSSL emits
		// OBJ_undef as if it were zero.
		oid := obj.oid
		if nid == 0 {
			oid = []int{0}
		}
		if len(oid) != 0 {
			var oidStr string
			for _, val := range oid {
				if len(oidStr) != 0 {
					oidStr += ","
				}
				oidStr += fmt.Sprintf("%dL", val)
			}

			fmt.Fprintf(&b, "#define OBJ_%s %s\n", obj.name, oidStr)
		}

		fmt.Fprintf(&b, "\n")
	}

	fmt.Fprintf(&b, `
#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_NID_H */
`)

	formatted, err := clangFormat(b.String())
	if err != nil {
		return err
	}

	return os.WriteFile(path, []byte(formatted), 0666)
}

func sortNIDs(nids []int, objs *objects, cmp func(a, b object) bool) {
	sort.Slice(nids, func(i, j int) bool { return cmp(objs.byNID[nids[i]], objs.byNID[nids[j]]) })
}

func writeData(path string, objs *objects) error {
	var b bytes.Buffer
	fmt.Fprintf(&b, `/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG `+"``"+`AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

/* This file is generated by crypto/obj/objects.go. */


`)

	fmt.Fprintf(&b, "#define NUM_NID %d\n", len(objs.byNID))

	// Emit each object's DER encoding, concatenated, and save the offsets.
	fmt.Fprintf(&b, "\nstatic const uint8_t kObjectData[] = {\n")
	offsets := make([]int, len(objs.byNID))
	var nextOffset int
	for nid, obj := range objs.byNID {
		if len(obj.name) == 0 || len(obj.encoded) == 0 {
			offsets[nid] = -1
			continue
		}

		offsets[nid] = nextOffset
		nextOffset += len(obj.encoded)
		fmt.Fprintf(&b, "/* NID_%s */\n", obj.name)
		for _, val := range obj.encoded {
			fmt.Fprintf(&b, "0x%02x, ", val)
		}
		fmt.Fprintf(&b, "\n")
	}
	fmt.Fprintf(&b, "};\n")

	// Emit an ASN1_OBJECT for each object.
	fmt.Fprintf(&b, "\nstatic const ASN1_OBJECT kObjects[NUM_NID] = {\n")
	for nid, obj := range objs.byNID {
		if len(obj.name) == 0 {
			fmt.Fprintf(&b, "{NULL, NULL, NID_undef, 0, NULL, 0},\n")
			continue
		}

		fmt.Fprintf(&b, "{\"%s\", \"%s\", NID_%s, ", obj.shortName, obj.longName, obj.name)
		if offset := offsets[nid]; offset >= 0 {
			fmt.Fprintf(&b, "%d, &kObjectData[%d], 0},\n", len(obj.encoded), offset)
		} else {
			fmt.Fprintf(&b, "0, NULL, 0},\n")
		}
	}
	fmt.Fprintf(&b, "};\n")

	// Emit a list of NIDs sorted by short name.
	var nids []int
	for nid, obj := range objs.byNID {
		if len(obj.name) == 0 || len(obj.shortName) == 0 {
			continue
		}
		nids = append(nids, nid)
	}
	sortNIDs(nids, objs, func(a, b object) bool { return a.shortName < b.shortName })

	fmt.Fprintf(&b, "\nstatic const uint16_t kNIDsInShortNameOrder[] = {\n")
	for _, nid := range nids {
		fmt.Fprintf(&b, "%d /* %s */,\n", nid, objs.byNID[nid].shortName)
	}
	fmt.Fprintf(&b, "};\n")

	// Emit a list of NIDs sorted by long name.
	nids = nil
	for nid, obj := range objs.byNID {
		if len(obj.name) == 0 || len(obj.longName) == 0 {
			continue
		}
		nids = append(nids, nid)
	}
	sortNIDs(nids, objs, func(a, b object) bool { return a.longName < b.longName })

	fmt.Fprintf(&b, "\nstatic const uint16_t kNIDsInLongNameOrder[] = {\n")
	for _, nid := range nids {
		fmt.Fprintf(&b, "%d /* %s */,\n", nid, objs.byNID[nid].longName)
	}
	fmt.Fprintf(&b, "};\n")

	// Emit a list of NIDs sorted by OID.
	nids = nil
	for nid, obj := range objs.byNID {
		if len(obj.name) == 0 || len(obj.encoded) == 0 {
			continue
		}
		nids = append(nids, nid)
	}
	sortNIDs(nids, objs, func(a, b object) bool {
		// This comparison must match the definition of |obj_cmp|.
		if len(a.encoded) < len(b.encoded) {
			return true
		}
		if len(a.encoded) > len(b.encoded) {
			return false
		}
		return bytes.Compare(a.encoded, b.encoded) < 0
	})

	fmt.Fprintf(&b, "\nstatic const uint16_t kNIDsInOIDOrder[] = {\n")
	for _, nid := range nids {
		obj := objs.byNID[nid]
		fmt.Fprintf(&b, "%d /* ", nid)
		for i, c := range obj.oid {
			if i > 0 {
				fmt.Fprintf(&b, ".")
			}
			fmt.Fprintf(&b, "%d", c)
		}
		fmt.Fprintf(&b, " (OBJ_%s) */,\n", obj.name)
	}
	fmt.Fprintf(&b, "};\n")

	formatted, err := clangFormat(b.String())
	if err != nil {
		return err
	}

	return os.WriteFile(path, []byte(formatted), 0666)
}

func main() {
	objs, err := readObjects("obj_mac.num", "objects.txt")
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error reading objects: %s\n", err)
		os.Exit(1)
	}

	if err := writeNumbers("obj_mac.num", objs); err != nil {
		fmt.Fprintf(os.Stderr, "Error writing numbers: %s\n", err)
		os.Exit(1)
	}

	if err := writeHeader("../../include/openssl/nid.h", objs); err != nil {
		fmt.Fprintf(os.Stderr, "Error writing header: %s\n", err)
		os.Exit(1)
	}

	if err := writeData("obj_dat.h", objs); err != nil {
		fmt.Fprintf(os.Stderr, "Error writing data: %s\n", err)
		os.Exit(1)
	}
}
