//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: Failure (Error message):
//  expected:
//  should match '/expected magic word.*@\+0/'
//  found:
//  "WebAssembly.Module doesn't parse at byte 0: module doesn't start with '\\0asm' (evaluating 'new WebAssembly.Module(buffer)')"
// Looks like we need to update the error string.

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --expose-wasm

'use strict';

load("wasm-module-builder.js");

function testErrorPositionAsyncOnly(bytes, pos, message) {
  let buffer = bytes.trunc_buffer();
  // Only test the streaming decoder since this kind of error is out of sync
  // with the non-streaming decoder, hence errors cannot be compared.
  assertThrowsAsync(
      WebAssembly.compile(buffer), WebAssembly.CompileError,
      new RegExp(message + '.*@\\+' + pos));
}

function testErrorPosition(bytes, pos, message) {
  let buffer = bytes.trunc_buffer();
  // First check the non-streaming decoder as a reference.
  assertThrows(
      () => new WebAssembly.Module(buffer), WebAssembly.CompileError,
      new RegExp(message + '.*@\\+' + pos));
  // Next test the actual streaming decoder.
  assertThrowsAsync(
      WebAssembly.compile(buffer), WebAssembly.CompileError,
      new RegExp(message + '.*@\\+' + pos));
}

(function testInvalidMagic() {
  let bytes = new Binary;
  bytes.emit_bytes([
    kWasmH0, kWasmH1 + 1, kWasmH2, kWasmH3, kWasmV0, kWasmV1, kWasmV2, kWasmV3
  ]);
  // Error at pos==0 because that's where the magic word is.
  testErrorPosition(bytes, 0, 'expected magic word');
})();

(function testInvalidVersion() {
  let bytes = new Binary;
  bytes.emit_bytes([
    kWasmH0, kWasmH1, kWasmH2, kWasmH3, kWasmV0, kWasmV1 + 1, kWasmV2, kWasmV3
  ]);
  // Error at pos==4 because that's where the version word is.
  testErrorPosition(bytes, 4, 'expected version');
})();

(function testSectionLengthInvalidVarint() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_u8(kTypeSectionCode);
  bytes.emit_bytes([0x80, 0x80, 0x80, 0x80, 0x80, 0x00]);
  let pos = bytes.length - 1 - 1;
  testErrorPosition(bytes, pos, 'expected section length');
})();

(function testSectionLengthTooBig() {
  let bytes = new Binary;
  bytes.emit_header();
  let pos = bytes.length;
  bytes.emit_u8(kTypeSectionCode);
  bytes.emit_u32v(0xffffff23);
  testErrorPositionAsyncOnly(
      bytes, pos,
      'section \\(code 1, "Type"\\) extends past end of the module ' +
          '\\(length 4294967075, remaining bytes 0\\)');
})();

(function testFunctionsCountInvalidVarint() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,  // section id
      1,                 // section length
      0                  // number of types
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      1,                     // section length
      0                      // number of functions
  ]);
  let pos = bytes.length;
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
  ]);
  // Functions count
  bytes.emit_bytes([0x80, 0x80, 0x80, 0x80, 0x80, 0x00]);

  testErrorPositionAsyncOnly(
      bytes, pos,
      'section \\(code 10, "Code"\\) extends past end of the module ' +
          '\\(length 20, remaining bytes 6\\)');
})();

(function testFunctionsCountTooBig() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,  // section id
      1,                 // section length
      0                  // number of types
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      1,                     // section length
      0                      // number of functions
  ]);
  let pos = bytes.length;
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
  ]);
  // Functions count
  bytes.emit_u32v(0xffffff23);

  testErrorPositionAsyncOnly(
      bytes, pos,
      'section \\(code 10, "Code"\\) extends past end of the module ' +
          '\\(length 20, remaining bytes 5\\)');
})();

(function testFunctionsCountDoesNotMatch() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,  // section id
      1,                 // section length
      0                  // number of types
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      1,                     // section length
      0                      // number of functions
  ]);
  let pos = bytes.length;
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
  ]);
  // Functions count (different than the count in the functions section.
  bytes.emit_u32v(5);

  testErrorPositionAsyncOnly(
      bytes, pos,
      'section \\(code 10, "Code"\\) extends past end of the module ' +
          '\\(length 20, remaining bytes 1\\)');
})();

(function testBodySizeInvalidVarint() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  let pos = bytes.length;
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
      1                  // functions count
  ]);
  // Invalid function body size.
  bytes.emit_bytes([0x80, 0x80, 0x80, 0x80, 0x80, 0x00]);

  testErrorPositionAsyncOnly(
      bytes, pos,
      'section \\(code 10, "Code"\\) extends past end of the module ' +
          '\\(length 20, remaining bytes 7\\)');
})();

(function testBodySizeTooBig() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  let pos = bytes.length;
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
      1                  // functions count
  ]);
  // Invalid function body size.
  bytes.emit_u32v(0xffffff23);

  testErrorPositionAsyncOnly(
      bytes, pos,
      'section \\(code 10, "Code"\\) extends past end of the module ' +
          '\\(length 20, remaining bytes 6\\)');
})();

(function testBodySizeDoesNotFit() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  let pos = bytes.length;
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
      1                  // functions count
  ]);
  // Invalid function body size (does not fit into the code section).
  bytes.emit_u32v(20);

  testErrorPositionAsyncOnly(
      bytes, pos,
      'section \\(code 10, "Code"\\) extends past end of the module ' +
          '\\(length 20, remaining bytes 2\\)');
})();

(function testBodySizeIsZero() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  let pos = bytes.length;
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
      1                  // functions count
  ]);
  // Invalid function body size (body size of 0 is invalid).
  bytes.emit_u32v(0);

  testErrorPositionAsyncOnly(
      bytes, pos,
      'section \\(code 10, "Code"\\) extends past end of the module ' +
          '\\(length 20, remaining bytes 2\\)');
})();

(function testStaleCodeSectionBytes() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      10,                // section length (too big)
      1,                 // functions count
      2,                 // body size
      0,                 // locals count
      kExprEnd           // body
  ]);
  let pos = bytes.length;
  // Add some more bytes to avoid early error detection for too big section
  // length.
  bytes.emit_bytes([0, 0, 0, 0, 0, 0, 0, 0]);

  testErrorPositionAsyncOnly(
      bytes, pos,
      'section was shorter than expected size ' +
          '\\(10 bytes expected, 4 decoded\\)');
})();

(function testInvalidCode() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      6,                 // section length
      1,                 // functions count
      4,                 // body size
      0,                 // locals count
      kExprLocalGet, 0,  // Access a non-existing local
      kExprEnd           // --
  ]);

  // Find error at the index of kExprLocalGet.
  let pos = bytes.length - 1 - 1;
  testErrorPosition(bytes, pos, 'invalid local index');
})();

(function testCodeSectionRepeats() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      4,                 // section length
      1,                 // functions count
      2,                 // body size
      0,                 // locals count
      kExprEnd           // body
  ]);
  // TODO(clemensb): Fix error reporting to point to the section start, not the
  // payload start.
  let pos = bytes.length + 2;
  bytes.emit_bytes([
      kCodeSectionCode,  // section id (repeating)
      4,                 // section length
      1,                 // functions count
      2,                 // body size
      0,                 // locals count
      kExprEnd           // body
  ]);

  // Find error at the second kCodeSectionCode.
  testErrorPositionAsyncOnly(bytes, pos, 'unexpected section <Code>');
})();

(function testCodeSectionSizeZero() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      0,                 // section length (empty)
  ]);

  // Find error at the code section length.
  let pos = bytes.length;
  testErrorPositionAsyncOnly(bytes, pos, 'expected functions count');
})();

(function testInvalidSection() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      5,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      1,                      // number of parameter
      kWasmVoid,              // invalid type
      0                       // number of returns
  ]);

  let pos = bytes.length - 1 - 1;
  testErrorPositionAsyncOnly(bytes, pos, 'invalid value type');
})();
