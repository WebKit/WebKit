//@ requireOptions("--useWasmJSStringBuiltins=true")

import * as assert from '../assert.js';
import { watToWasm } from "../wabt-wrapper.js";
import { watToWasm as gc_watToWasm } from "../gc/wast-wrapper.js";

const interestingStrings = [
  '',
  'ascii',
  'latin\xa91',        // Latin-1.
  '2 \ucccc b',        // Two-byte.
  'a \ud800\udc00 b',  // Proper surrogate pair.
  'a \ud800 b',        // Lone lead surrogate.
  'a \udc00 b',        // Lone trail surrogate.
  '\ud800 bc',         // Lone lead surrogate at the start.
  '\udc00 bc',         // Lone trail surrogate at the start.
  'ab \ud800',         // Lone lead surrogate at the end.
  'ab \udc00',         // Lone trail surrogate at the end.
  'a \udc00\ud800 b',  // Swapped surrogate pair.
];

function assertThrowsIllegalArgument(fun, ...args) {
    assert.throws(fun, WebAssembly.RuntimeError, "Illegal argument", ...args);
}

// Exercise all available pathways for instantiation with compileOptions introduced by the proposal
async function testInstantiation() {
    const wat = `
    (module
        (import "wasm:js-string" "concat" (func $concatBuiltin (param externref externref) (result externref)))
        (export "foo" (func $concatBuiltin))
    )`;
    const buffer = await watToWasm(wat);

    // bytes -> module -> instance
    const module1 = new WebAssembly.Module(buffer, { builtins: ['js-string'] });
    const instance1 = new WebAssembly.Instance(module1, {});
    assert.isNotUndef(instance1.exports.foo);

    // bytes --async WA--> module -> instance
    const module2 = await WebAssembly.compile(buffer, { builtins: ['js-string'] });
    const instance2 = new WebAssembly.Instance(module2, {});
    assert.isNotUndef(instance2.exports.foo);

    // bytes --async WA--> instance
    const instantiatedSource = await WebAssembly.instantiate(buffer, {}, { builtins: ['js-string'] });
    const instance3 = instantiatedSource.instance;
    assert.isNotUndef(instance3.exports.foo);
}

async function testInstantiationWithEmptyCompileOptions() {
    const wat = `
    (module
        (func (export "foo") (result i32)
            i32.const 42
        )
    )`;
    const buffer = await watToWasm(wat);

    // bytes -> module -> instance
    const module1 = new WebAssembly.Module(buffer, { });
    const instance1 = new WebAssembly.Instance(module1, { });
    assert.isNotUndef(instance1.exports.foo);

    // bytes --async WA--> module -> instance
    const module2 = await WebAssembly.compile(buffer, { });
    const instance2 = new WebAssembly.Instance(module2, { });
    assert.isNotUndef(instance2.exports.foo);

    // bytes --async WA--> instance
    const instantiatedSource = await WebAssembly.instantiate(buffer, { }, { });
    const instance3 = instantiatedSource.instance;
    assert.isNotUndef(instance3.exports.foo);
}

// Two convenience wat -> instance functions
// which instantiate with js-builtins enabled.

async function instantiate(wat) {
    const buffer = await watToWasm(wat);
    const result = await WebAssembly.instantiate(buffer, {}, {
        builtins: ['js-string'],
        importedStringConstants: "const"
    });
    return result.instance;
}

// Instantiate using the GC-aware wat-to-wasm compiler.
async function gc_instantiate(wat) {
    const buffer = await gc_watToWasm(wat);
    const result = await WebAssembly.instantiate(buffer, {}, {
        builtins: ['js-string'],
        importedStringConstants: "const"
    });
    return result.instance;
}

async function testCast() {
    const wat = `
    (module
        (import "wasm:js-string" "cast" (func $builtin (param externref) (result externref)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $arg externref) (result externref)
            local.get $arg
            call $builtin
        )
    )`;
    const instance = await instantiate(wat);

    function check(fun) {
        assert.eq("foobar", fun("foobar"));
        assert.eq("", fun(""));
        assertThrowsIllegalArgument(fun, 42);
        assertThrowsIllegalArgument(fun, undefined);
        assertThrowsIllegalArgument(fun, true);
        assertThrowsIllegalArgument(fun, false);
        assertThrowsIllegalArgument(fun, null);
    }

    // Run a bunch of times to exercise higher tiers
    for (let i = 0; i < 10000; i++) {
        check(instance.exports.relay);
    }
    check(instance.exports.exported);
}

async function testTest() {
    const wat = `
    (module
        (import "wasm:js-string" "test" (func $builtin (param externref) (result i32)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $arg externref) (result i32)
            local.get $arg
            call $builtin
        )
    )`;
    const instance = await instantiate(wat);

    function check(fun) {
        assert.eq(1, fun("foobar"));
        assert.eq(1, fun(""));
        assert.eq(0, fun({}));
        assert.eq(0, fun([]));
        assert.eq(0, fun(42));
        assert.eq(0, fun(true));
        assert.eq(0, fun(false));
        assert.eq(0, fun(null));
        assert.eq(0, fun(undefined));
    }

    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testFromCharCodeArray() {
    const wat = `
    (module
        (type $arrayMutI16 (array (mut i16)))
        (import "wasm:js-string" "fromCharCodeArray" (func $builtin (param (ref null $arrayMutI16) i32 i32) (result externref)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $array (ref null $arrayMutI16)) (param $start i32) (param $end i32) (result externref)
            local.get $array
            local.get $start
            local.get $end
            call $builtin
        )
        (func (export "makeArray") (param $arg i32) (result (ref $arrayMutI16))
            local.get $arg
            (array.new_default $arrayMutI16)
        )
        (func (export "arraySet") (param $array (ref $arrayMutI16)) (param $index i32) (param $value i32)
            local.get $array
            local.get $index
            local.get $value
            (array.set $arrayMutI16)
        )
    )`;
    const instance = await gc_instantiate(wat);

    function makeArrayWithChars(string) {
        const array = instance.exports.makeArray(string.length);
        for (let i = 0; i < string.length; i++) {
            const charCode = string.charCodeAt(i);
            instance.exports.arraySet(array, i, charCode);
        }
        return array;
    }

    const helloArray = makeArrayWithChars("Hello");
    const smileyArray = makeArrayWithChars("😀😦"); // length == 4

    function check(fun) {
        assert.eq("Hello", fun(helloArray, 0, 5));
        assert.eq("ello", fun(helloArray, 1, 5));
        assert.eq("llo", fun(helloArray, 2, 5));
        assert.eq("ell", fun(helloArray, 1, 4));
        assert.eq("ll", fun(helloArray, 2, 4));
        assert.eq("H", fun(helloArray, 0, 1));
        assert.eq("l", fun(helloArray, 2, 3));
        assert.eq("o", fun(helloArray, 4, 5));
        assert.eq("", fun(helloArray, 0, 0));
        assert.eq("", fun(helloArray, 2, 2));
        assert.eq("😀😦", fun(smileyArray, 0, 4));
        assert.eq("😀", fun(smileyArray, 0, 2));
        assert.eq("😦", fun(smileyArray, 2, 4));

        // Null array argument
        assertThrowsIllegalArgument(fun, null, 0, 5);
        // Invalid indices
        assertThrowsIllegalArgument(fun, helloArray, 3, 1);
        assertThrowsIllegalArgument(fun, helloArray, 0, 6);
        assertThrowsIllegalArgument(fun, helloArray, 8, 6);
        assertThrowsIllegalArgument(fun, helloArray, 2, 1);
        assertThrowsIllegalArgument(fun, helloArray, -1, 1);
        assertThrowsIllegalArgument(fun, helloArray, 3, -1);
        assertThrowsIllegalArgument(fun, helloArray, -10, -8);
    }

    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testIntoCharCodeArray() {
    const wat = `
    (module
        (type $arrayMutI16 (array (mut i16)))
        (import "wasm:js-string" "intoCharCodeArray" (func $builtinInto (param externref (ref null $arrayMutI16) i32) (result i32)))
        (import "wasm:js-string" "fromCharCodeArray" (func $builtinFrom (param (ref null $arrayMutI16) i32 i32) (result externref)))
        (export "exported" (func $builtinInto))
        (func (export "from") (param $array (ref null $arrayMutI16)) (param $start i32) (param $end i32) (result externref)
            local.get $array
            local.get $start
            local.get $end
            call $builtinFrom
        )
        (func (export "into") (param $string externref) (param $array (ref null $arrayMutI16)) (param $start i32) (result i32)
            local.get $string
            local.get $array
            local.get $start
            call $builtinInto
        )
        (func (export "makeArray") (param $arg i32) (result (ref $arrayMutI16))
            local.get $arg
            (array.new_default $arrayMutI16)
        )
        (func (export "arrayGet") (param $array (ref $arrayMutI16)) (param $index i32) (result i32)
            (array.get_u $arrayMutI16 (local.get $array) (local.get $index))
        )
    )`;
    const instance = await gc_instantiate(wat);
    const into = instance.exports.into;
    const from = instance.exports.from;
    const arrayGet = instance.exports.arrayGet;
    const array = instance.exports.makeArray(5);

    function check(fun) {
        assert.eq(5, fun("Hello", array, 0));
        assert.eq("Hello", from(array, 0, 5));

        assert.eq(3, fun("fty", array, 2));
        assert.eq("Hefty", from(array, 0, 5));

        // should return string length regardless of start position
        assert.eq(1, fun("x", array, 2));
        assert.eq("Hexty", from(array, 0, 5));
        assert.eq(1, fun("x", array, 3));
        assert.eq(1, fun("x", array, 4));

        // should handle a null character
        assert.eq(3, fun("A\0B", array, 1));
        assert.eq(65, arrayGet(array, 1)); // 'A'
        assert.eq(0, arrayGet(array, 2));
        assert.eq(66, arrayGet(array, 3)); // 'B'
        assert.eq(120, arrayGet(array, 4)); // 'o'

        // Non-ASCII Latin-1
        assert.eq(5, fun("Héllø", array, 0));
        assert.eq(233, arrayGet(array, 1));
        assert.eq(248, arrayGet(array, 4));
        assert.eq("Héllø", from(array, 0, 5));

        // two 2-charCodes: code and surrogate
        assert.eq(4, fun("😀😦", array, 1));
        assert.eq("H😀😦", from(array, 0, 5));

        // Unicode
        assert.eq(2, fun("東京", array, 0));
        assert.eq(0x6771, arrayGet(array, 0));
        assert.eq(0x4EAC, arrayGet(array, 1));
        assert.eq("東京", from(array, 0, 2));

        // max 16-bit value
        assert.eq(1, fun(String.fromCharCode(0xFFFF), array, 0));
        assert.eq(0xFFFF, arrayGet(array, 0));

        // Invalid negative start index
        assertThrowsIllegalArgument(fun, "Hello", array, -1);

        assertThrowsIllegalArgument(fun, null, array, 0);
        assertThrowsIllegalArgument(fun, null, array, 4);
        assertThrowsIllegalArgument(fun, {}, array, 0);
        assertThrowsIllegalArgument(fun, ['a', 'b'], array, 0);
        assertThrowsIllegalArgument(fun, 42, array, 0);

        assertThrowsIllegalArgument(fun, "", null, 0);
        assertThrowsIllegalArgument(fun, "Hello", null, 0);
        assertThrowsIllegalArgument(fun, "Hello", null, 4);
        assertThrowsIllegalArgument(fun, "", null, 0);

        assertThrowsIllegalArgument(fun, "Oversize string", array, 0);
        assertThrowsIllegalArgument(fun, "abc", array, 3); // just out of bounds
        assertThrowsIllegalArgument(fun, "abc", array, 11); // far out of bounds
    }

    check(into);
    check(instance.exports.exported);
}

async function testFromCharCode() {
    const wat = `
    (module
        (import "wasm:js-string" "fromCharCode" (func $builtin (param i32) (result externref)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $arg i32) (result externref)
            local.get $arg
            call $builtin
        )
    )`;
    const instance = await instantiate(wat);

    function check(fun) {
        assert.eq("a", fun(97));
        assert.eq("c", fun(99));
        assert.eq("\uFFFF", fun(0xFFFF));
        assert.eq(fun(0x1_0000), fun(0x11_0000)); // out of range value coersion
    }

    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testFromCodePoint() {
    const wat = `
    (module
        (import "wasm:js-string" "fromCodePoint" (func $builtin (param i32) (result externref)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $arg i32) (result externref)
            local.get $arg
            call $builtin
        )
    )`;
    const instance = await instantiate(wat);

    function check(fun) {
        assert.eq("a", fun(97));
        assert.eq("😀", fun(0x1F600));
        assert.eq("\uFFFF", fun(0xFFFF));
        assert.eq("\uD800", fun(0xD800)); // high surrogate, unusual standalone but valid
        assert.eq("\uDC00", fun(0xDC00)); // low surrogate
        assert.eq("\u0000", fun(0));
        assertThrowsIllegalArgument(fun, 0x110000); // out of range
    }
    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testCharCodeAt() {
    const wat = `
    (module
        (import "wasm:js-string" "charCodeAt" (func $builtin (param externref i32) (result i32)))
        (import "wasm:js-string" "concat" (func $concat (param externref externref) (result externref)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $arg externref) (param $len i32) (result i32)
            local.get $arg
            local.get $len
            call $builtin
        )
        (func (export "concat") (param $left externref) (param $right externref) (result externref)
            local.get $left
            local.get $right
            call $concat
        )
    )`;
    const instance = await instantiate(wat);
    const concat = instance.exports.concat;

    const string = "ab😀c";
    const string2 = concat("a😀", "β😦"); // makes a rope
    function check(fun) {
        assert.eq(97, fun(string, 0));
        assert.eq(98, fun(string, 1));
        assert.eq(0xD83D, fun(string, 2));
        assert.eq(0xDE00, fun(string, 3));
        assert.eq(99, fun(string, 4));
        assertThrowsIllegalArgument(fun, string, 5);
        assertThrowsIllegalArgument(fun, string, -1);
        assertThrowsIllegalArgument(fun, 42, 0);

        assert.eq(97, fun(string2, 0));
        assert.eq(0xD83D, fun(string2, 1));
        assert.eq(0xDE00, fun(string2, 2));
        assert.eq(0x3B2, fun(string2, 3));
        assert.eq(0xD83D, fun(string2, 4));
        assert.eq(0xDE26, fun(string2, 5));
        assertThrowsIllegalArgument(fun, string, 6);

        assertThrowsIllegalArgument(fun, null, 0);
        assertThrowsIllegalArgument(fun, 1234, 0);
        assertThrowsIllegalArgument(fun, "", 0);
    }

    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testCodePointAt() {
    const wat = `
    (module
        (import "wasm:js-string" "codePointAt" (func $builtin (param externref i32) (result i32)))
        (import "wasm:js-string" "concat" (func $concat (param externref externref) (result externref)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $arg externref) (param $len i32) (result i32)
            local.get $arg
            local.get $len
            call $builtin
        )
        (func (export "concat") (param $left externref) (param $right externref) (result externref)
            local.get $left
            local.get $right
            call $concat
        )
    )`;
    const instance = await instantiate(wat);
    const concat = instance.exports.concat;

    const string = 'a😀bΩ';
    const string2 = concat("a😀", "β😦"); // makes a rope
    function check(fun) {
        assert.eq(97, fun(string, 0));
        assert.eq(0x1F600, fun(string, 1));
        assert.eq(0xDE00, fun(string, 2)); // low surrogate of 😀
        assert.eq(98, fun(string, 3));
        assert.eq(937, fun(string, 4));
        assertThrowsIllegalArgument(fun, string, 5);
        assertThrowsIllegalArgument(fun, string, -1);
        assertThrowsIllegalArgument(fun, 42, 0);

        assert.eq(97, fun(string2, 0));
        assert.eq(0x1F600, fun(string2, 1));
        assert.eq(0xDE00, fun(string2, 2));
        assert.eq(0x3B2, fun(string2, 3));
        assert.eq(0x1F626, fun(string2, 4));
        assert.eq(0xDE26, fun(string2, 5)); // low surrogate of 😦
        assertThrowsIllegalArgument(fun, string, 6);

        assertThrowsIllegalArgument(fun, null, 0);
        assertThrowsIllegalArgument(fun, "", 0);
    }

    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testConcat() {
    const wat = `
    (module
        (import "wasm:js-string" "concat" (func $builtin (param externref externref) (result externref)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $left externref) (param $right externref) (result externref)
            local.get $left
            local.get $right
            call $builtin
        )
    )`;
    const instance = await instantiate(wat);

    function check(fun) {
        assert.eq("foobar", fun("foo", "bar"));
        assert.eq("foobar", fun("", "foobar"));
        assert.eq("foobar", fun("foobar", ""));

        for (let left of interestingStrings) {
            for (let right of interestingStrings) {
                assert.eq(left + right, fun(left, right));
            }
        }

        assertThrowsIllegalArgument(fun, "foo", 42);
        assertThrowsIllegalArgument(fun, "foo", null);
        assertThrowsIllegalArgument(fun, 42, "foo");
        assertThrowsIllegalArgument(fun, null, "foo");
    }

    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testLength() {
    const wat = `
    (module
        (import "wasm:js-string" "length" (func $builtin (param externref) (result i32)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $arg externref) (result i32)
            local.get $arg
            call $builtin
        )
    )`;
    const instance = await instantiate(wat);

    function check(fun) {
        assert.eq(6, fun("foobar"));
        assert.eq(0, fun(""));

        for (let each of interestingStrings) {
            assert.eq(each.length, fun(each));
        }

        assertThrowsIllegalArgument(fun, 42);
        assertThrowsIllegalArgument(fun, null);
    }

    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testSubstring() {
    const wat = `
    (module
        (import "wasm:js-string" "substring" (func $builtin (param externref i32 i32) (result externref)))
        (import "wasm:js-string" "concat" (func $concat (param externref externref) (result externref)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $string externref) (param $start i32) (param $end i32) (result externref)
            local.get $string
            local.get $start
            local.get $end
            call $builtin
        )
        (func (export "concat") (param $left externref) (param $right externref) (result externref)
            local.get $left
            local.get $right
            call $concat
        )
    )`;
    const instance = await instantiate(wat);
    const concat = instance.exports.concat;

    const string = "Hello, world";
    const string2 = concat("Hello", ", world");
    function check(fun) {
        // Normal cases
        assert.eq("lo, w", fun(string, 3, 8));
        assert.eq("lo, w", fun(string2, 3, 8));
        assert.eq("", fun(string, 3, 3));
        assert.eq("", fun(string2, 3, 3));
        assert.eq("", fun(string, 0, 0));
        assert.eq("", fun(string2, 0, 0));
        assert.eq("d", fun(string, 11, 12));
        assert.eq("d", fun(string2, 11, 12));
        assert.eq("d", fun(string, 11, 13));
        assert.eq("d", fun(string2, 11, 13));
        assert.eq("", fun(string, 12, 12));
        assert.eq("", fun(string2, 12, 12));
        // Indices outside bounds
        assert.eq("He", fun(string, -2, 2));
        assert.eq("He", fun(string2, -2, 2));
        assert.eq("orld", fun(string, 8, 20));
        assert.eq("orld", fun(string2, 8, 20));
        assert.eq("Hello, world", fun(string, -100, 100));
        assert.eq("Hello, world", fun(string2, -100, 100));
        // start > end and start > length
        assert.eq("", fun(string, 3, 2));
        assert.eq("", fun(string, 20, 25));
        assert.eq("", fun(string, -3, -2));
        assert.eq("", fun(string, -2, -3));
        assert.eq("", fun(string2, 3, 2));
        assert.eq("", fun(string2, 20, 25));
        assert.eq("", fun(string2, -3, -2));
        assert.eq("", fun(string2, -2, -3));
        // Unicode ouside MBP
        assert.eq("😀", fun("a😀", 1, 3));
    }

    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testEquals() {
    const wat = `
    (module
        (import "wasm:js-string" "equals" (func $builtin (param externref externref) (result i32)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $left externref) (param $right externref) (result i32)
            local.get $left
            local.get $right
            call $builtin
        )
    )`;
    const instance = await instantiate(wat);

    function check(fun) {
        assert.eq(1, fun("foo", "foo"));
        assert.eq(1, fun("bar", "bar"));
        assert.eq(0, fun("foo", "bar"));
        assert.eq(0, fun("bar", "foo"));
        assert.eq(1, fun(null, null));

        for (let left of interestingStrings) {
            for (let right of interestingStrings) {
                const expected = (left == right) | 0;
                assert.eq(expected, fun(left, right));
            }
        }

        assertThrowsIllegalArgument(fun, null, "foo");
        assertThrowsIllegalArgument(fun, "foo", null);
        assertThrowsIllegalArgument(fun, "foo", 42);
        assertThrowsIllegalArgument(fun, 42, "foo");
    }

    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testCompare() {
    const wat = `
    (module
        (import "wasm:js-string" "compare" (func $builtin (param externref externref) (result i32)))
        (export "exported" (func $builtin))
        (func (export "relay") (param $left externref) (param $right externref) (result i32)
            local.get $left
            local.get $right
            call $builtin
        )
    )`;
    const instance = await instantiate(wat);

    function check(fun) {
        assert.eq(1, fun("foo", "bar"));
        assert.eq(0, fun("foo", "foo"));
        assert.eq(-1, fun("bar", "foo"));

        for (let left of interestingStrings) {
            for (let right of interestingStrings) {
                const expected = left < right ? -1 : left > right ? 1 : 0;
                assert.eq(expected, fun(left, right));
            }
        }

        assertThrowsIllegalArgument(fun, "foo", null);
        assertThrowsIllegalArgument(fun, null, "foo");
    }

    check(instance.exports.relay);
    check(instance.exports.exported);
}

async function testImportedStringConstants() {
    const wat = `
    (module
        (import "const" "this is constant 1" (global $const1 externref))
        (import "const" "this is constant 2" (global $const2 externref))
        (export "exportedConst2" (global $const2))
        (func (export "returnConst1") (result externref)
            global.get $const1
        )
    )`;
    const instance = await instantiate(wat);

    assert.eq("this is constant 1", instance.exports.returnConst1());
    assert.eq("this is constant 2", instance.exports.exportedConst2.value);
}

await assert.asyncTest(testInstantiation());
await assert.asyncTest(testInstantiationWithEmptyCompileOptions());
await assert.asyncTest(testCast());
await assert.asyncTest(testTest());
await assert.asyncTest(testFromCharCodeArray());
await assert.asyncTest(testIntoCharCodeArray());
await assert.asyncTest(testFromCharCode());
await assert.asyncTest(testFromCodePoint());
await assert.asyncTest(testCharCodeAt());
await assert.asyncTest(testCodePointAt());
await assert.asyncTest(testLength());
await assert.asyncTest(testConcat());
await assert.asyncTest(testSubstring());
await assert.asyncTest(testEquals());
await assert.asyncTest(testCompare());
await assert.asyncTest(testImportedStringConstants());
