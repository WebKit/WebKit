//@ skip if $addressBits <= 32
//@ requireOptions("--useWasmJSStringBuiltins=true")

// Test case from rdar://166830652.
// The return type of js-string builtins cast, fromCharCode, fromCodePoint, concat and substring
// is supposed to be (ref extern) instead of externref (aka `(ref null extern)`).
// This is because the JS equivalents of those methods always return a string and never null.

import * as assert from '../assert.js';

async function test() {
    /*
        (module
            (import "wasm:js-string" "cast" (func $cast (param externref) (result (ref extern))))
            (import "wasm:js-string" "test" (func $test (param externref) (result i32)))
            (import "wasm:js-string" "fromCharCode" (func $fromCharCode (param i32) (result (ref extern))))
            (import "wasm:js-string" "fromCodePoint" (func $fromCodePoint (param i32) (result (ref extern))))
            (import "wasm:js-string" "charCodeAt" (func $charCodeAt (param externref i32) (result i32)))
            (import "wasm:js-string" "codePointAt" (func $codePointAt (param externref i32) (result i32)))
            (import "wasm:js-string" "length" (func $length (param externref) (result i32)))
            (import "wasm:js-string" "concat" (func $concat (param externref externref) (result (ref extern))))
            (import "wasm:js-string" "substring" (func $substring (param externref i32 i32) (result (ref extern))))
            (import "wasm:js-string" "equals" (func $equals (param externref externref) (result i32)))
            (import "wasm:js-string" "compare" (func $compare (param externref externref) (result i32)))
            (func (export "doublehalf") (param $str externref) (result (ref extern))
                (local.set $str
                    (call $substring
                        (local.get $str)
                        (i32.const 0)
                        (i32.div_u
                            (call $length
                                (local.get $str))
                            (i32.const 2))))
                (call $concat
                    (local.get $str)
                    (local.get $str))
            )
        )
    */
    let {instance} = await WebAssembly.instantiate(Uint8Array.fromBase64(
        "AGFzbQEAAAABLQdgAW8BZG9gAW8Bf2ABfwFkb2ACb38Bf2ACb28BZG9gA29/fwFkb2ACb28BfwKfAgsOd2FzbTpqcy1zdHJpbmcEY2FzdAAADndhc206anMtc3RyaW5nBHRlc3QAAQ53YXNtOmpzLXN0cmluZwxmcm9tQ2hhckNvZGUAAg53YXNtOmpzLXN0cmluZw1mcm9tQ29kZVBvaW50AAIOd2FzbTpqcy1zdHJpbmcKY2hhckNvZGVBdAADDndhc206anMtc3RyaW5nC2NvZGVQb2ludEF0AAMOd2FzbTpqcy1zdHJpbmcGbGVuZ3RoAAEOd2FzbTpqcy1zdHJpbmcGY29uY2F0AAQOd2FzbTpqcy1zdHJpbmcJc3Vic3RyaW5nAAUOd2FzbTpqcy1zdHJpbmcGZXF1YWxzAAYOd2FzbTpqcy1zdHJpbmcHY29tcGFyZQAGAwIBAAcOAQpkb3VibGVoYWxmAAsKGQEXACAAQQAgABAGQQJuEAghACAAIAAQBwsAgAEEbmFtZQFvCwAEY2FzdAEEdGVzdAIMZnJvbUNoYXJDb2RlAw1mcm9tQ29kZVBvaW50BApjaGFyQ29kZUF0BQtjb2RlUG9pbnRBdAYGbGVuZ3RoBwZjb25jYXQICXN1YnN0cmluZwkGZXF1YWxzCgdjb21wYXJlAggBCwEAA3N0cg=="
    ), {}, {builtins: ["js-string"]});

    assert.eq(instance.exports.doublehalf("fhqwhgads"), "fhqwfhqw");

    /*
        (module
            (import "wasm:js-string" "cast" (func $cast (param externref) (result externref)))
            (import "wasm:js-string" "test" (func $test (param externref) (result i32)))
            (import "wasm:js-string" "fromCharCode" (func $fromCharCode (param i32) (result externref)))
            (import "wasm:js-string" "fromCodePoint" (func $fromCodePoint (param i32) (result externref)))
            (import "wasm:js-string" "charCodeAt" (func $charCodeAt (param externref i32) (result i32)))
            (import "wasm:js-string" "codePointAt" (func $codePointAt (param externref i32) (result i32)))
            (import "wasm:js-string" "length" (func $length (param externref) (result i32)))
            (import "wasm:js-string" "concat" (func $concat (param externref externref) (result externref)))
            (import "wasm:js-string" "substring" (func $substring (param externref i32 i32) (result externref)))
            (import "wasm:js-string" "equals" (func $equals (param externref externref) (result i32)))
            (import "wasm:js-string" "compare" (func $compare (param externref externref) (result i32)))
            (func (export "doublehalf") (param $str externref) (result externref)
                (local.set $str
                    (call $substring
                        (local.get $str)
                        (i32.const 0)
                        (i32.div_u
                            (call $length
                                (local.get $str))
                            (i32.const 2))))
                (call $concat
                    (local.get $str)
                    (local.get $str))
            )
        )
    */
    await assert.throwsAsync(WebAssembly.instantiate(
        Uint8Array.fromBase64(
            "AGFzbQEAAAABKQdgAW8Bb2ABbwF/YAF/AW9gAm9/AX9gAm9vAW9gA29/fwFvYAJvbwF/Ap8CCw53YXNtOmpzLXN0cmluZwRjYXN0AAAOd2FzbTpqcy1zdHJpbmcEdGVzdAABDndhc206anMtc3RyaW5nDGZyb21DaGFyQ29kZQACDndhc206anMtc3RyaW5nDWZyb21Db2RlUG9pbnQAAg53YXNtOmpzLXN0cmluZwpjaGFyQ29kZUF0AAMOd2FzbTpqcy1zdHJpbmcLY29kZVBvaW50QXQAAw53YXNtOmpzLXN0cmluZwZsZW5ndGgAAQ53YXNtOmpzLXN0cmluZwZjb25jYXQABA53YXNtOmpzLXN0cmluZwlzdWJzdHJpbmcABQ53YXNtOmpzLXN0cmluZwZlcXVhbHMABg53YXNtOmpzLXN0cmluZwdjb21wYXJlAAYDAgEABw4BCmRvdWJsZWhhbGYACwoZARcAIABBACAAEAZBAm4QCCEAIAAgABAHCwCAAQRuYW1lAW8LAARjYXN0AQR0ZXN0Agxmcm9tQ2hhckNvZGUDDWZyb21Db2RlUG9pbnQECmNoYXJDb2RlQXQFC2NvZGVQb2ludEF0BgZsZW5ndGgHBmNvbmNhdAgJc3Vic3RyaW5nCQZlcXVhbHMKB2NvbXBhcmUCCAELAQADc3Ry"
        ), {}, {builtins: ["js-string"]}),
        WebAssembly.CompileError,
        "builtin import wasm:js-string:cast has an unexpected signature");

    /*
        (module
            (import "wasm:js-string" "cast" (func $cast (param externref) (result externref)))
        )
    */
    await assert.throwsAsync(WebAssembly.instantiate(Uint8Array.fromBase64("AGFzbQEAAAABBgFgAW8BbwIXAQ53YXNtOmpzLXN0cmluZwRjYXN0AAAADgRuYW1lAQcBAARjYXN0"), {}, {builtins: ["js-string"]}),
        WebAssembly.CompileError, "builtin import wasm:js-string:cast has an unexpected signature");

    /*
        (module
            (import "wasm:js-string" "fromCharCode" (func $fromCharCode (param i32) (result externref)))
        )
    */
   await assert.throwsAsync(WebAssembly.instantiate(Uint8Array.fromBase64("AGFzbQEAAAABBgFgAX8BbwIfAQ53YXNtOmpzLXN0cmluZwxmcm9tQ2hhckNvZGUAAAAWBG5hbWUBDwEADGZyb21DaGFyQ29kZQ=="), {}, {builtins: ["js-string"]}),
        WebAssembly.CompileError, "builtin import wasm:js-string:fromCharCode has an unexpected signature");

    /*
        (module
            (import "wasm:js-string" "fromCodePoint" (func $fromCodePoint (param i32) (result externref)))
        )
    */
    await assert.throwsAsync(WebAssembly.instantiate(Uint8Array.fromBase64("AGFzbQEAAAABBgFgAX8BbwIgAQ53YXNtOmpzLXN0cmluZw1mcm9tQ29kZVBvaW50AAAAFwRuYW1lARABAA1mcm9tQ29kZVBvaW50"), {}, {builtins: ["js-string"]}),
        WebAssembly.CompileError, "builtin import wasm:js-string:fromCodePoint has an unexpected signature");

    /*
        (module
            (import "wasm:js-string" "concat" (func $concat (param externref externref) (result externref)))
        )
    */
    await assert.throwsAsync(WebAssembly.instantiate(Uint8Array.fromBase64("AGFzbQEAAAABBwFgAm9vAW8CGQEOd2FzbTpqcy1zdHJpbmcGY29uY2F0AAAAEARuYW1lAQkBAAZjb25jYXQ="), {}, {builtins: ["js-string"]}),
        WebAssembly.CompileError, "builtin import wasm:js-string:concat has an unexpected signature");

    /*
        (module
            (import "wasm:js-string" "substring" (func $substring (param externref i32 i32) (result externref)))
        )
    */
    await assert.throwsAsync(WebAssembly.instantiate(Uint8Array.fromBase64("AGFzbQEAAAABCAFgA29/fwFvAhwBDndhc206anMtc3RyaW5nCXN1YnN0cmluZwAAABMEbmFtZQEMAQAJc3Vic3RyaW5n"), {}, {builtins: ["js-string"]}),
        WebAssembly.CompileError, "builtin import wasm:js-string:substring has an unexpected signature");
}

await assert.asyncTest(test());
