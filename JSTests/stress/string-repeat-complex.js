function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected "${expected}", got "${actual}"`);
}

function shouldThrow(func, errorType) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (!(error instanceof errorType))
        throw new Error(`bad error type: expected ${errorType.name}, got ${error.constructor.name}`);
}

{
    shouldBe("".repeat(0), "");
    shouldBe("".repeat(-0), "");
    shouldBe("".repeat(1), "");
    shouldBe("".repeat(10), "");
    shouldBe("".repeat(100), "");
    shouldBe("".repeat(1000), "");
    shouldBe("".repeat(0xFFFFFF), "");
    shouldBe("".repeat(0xFFFFFFFF), "");
    shouldThrow(() => "".repeat(Infinity), RangeError);
    shouldThrow(() => "".repeat(-Infinity), RangeError);
    shouldThrow(() => "".repeat(-1), RangeError);
    shouldBe("".repeat(-0.1), "");
    shouldBe("".repeat(-0.9), "");
}

{
    shouldBe("a".repeat(0), "");
    shouldBe("ab".repeat(0), "");
    shouldBe("abc".repeat(0), "");
    shouldBe("abcdefgh".repeat(0), "");
    shouldBe("abcdefghi".repeat(0), "");
    shouldBe("a".repeat(-0), "");
    shouldBe("\u{1F600}".repeat(0), "");
    shouldBe("\u0100".repeat(0), "");
}

{
    shouldBe("a".repeat(1), "a");
    shouldBe("a".repeat(2), "aa");
    shouldBe("a".repeat(3), "aaa");
    shouldBe("a".repeat(7), "aaaaaaa");
    shouldBe("a".repeat(8), "aaaaaaaa");
    shouldBe("a".repeat(9), "aaaaaaaaa");
    shouldBe("a".repeat(100), "a".repeat(100));
    shouldBe("x".repeat(1000), "x".repeat(1000));
    shouldBe("a".repeat(1023).length, 1023);
    shouldBe("a".repeat(1024).length, 1024);
    shouldBe("a".repeat(1025).length, 1025);
    shouldBe("\x00".repeat(5), "\x00\x00\x00\x00\x00");
    shouldBe("\xFF".repeat(3), "\xFF\xFF\xFF");
    shouldBe(" ".repeat(10), "          ");
    shouldBe("\n".repeat(3), "\n\n\n");
    shouldBe("\t".repeat(4), "\t\t\t\t");
}

{
    shouldBe("\u0100".repeat(1), "\u0100");
    shouldBe("\u0100".repeat(3), "\u0100\u0100\u0100");
    shouldBe("\u0100".repeat(10).length, 10);
    shouldBe("\u4E2D".repeat(3), "\u4E2D\u4E2D\u4E2D");
    shouldBe("\u3042".repeat(4), "\u3042\u3042\u3042\u3042");
    shouldBe("\uFFFF".repeat(2), "\uFFFF\uFFFF");
    shouldBe("\u0100".repeat(1024).length, 1024);
    shouldBe("\u0100".repeat(1025).length, 1025);
}

{
    shouldBe("\u{1F600}".repeat(1), "\u{1F600}");
    shouldBe("\u{1F600}".repeat(2), "\u{1F600}\u{1F600}");
    shouldBe("\u{1F600}".repeat(3).length, 6);
    shouldBe("a\u{1F600}b".repeat(2), "a\u{1F600}ba\u{1F600}b");
}

{
    let str8 = "abcdefgh";
    shouldBe(str8.repeat(1), "abcdefgh");
    shouldBe(str8.repeat(2), "abcdefghabcdefgh");
    shouldBe(str8.repeat(128).length, 1024);
    shouldBe(str8.repeat(129).length, 1032);

    let str9 = "abcdefghi";
    shouldBe(str9.repeat(1), "abcdefghi");
    shouldBe(str9.repeat(2), "abcdefghiabcdefghi");
    shouldBe(str9.repeat(100).length, 900);

    let str20 = "12345678901234567890";
    shouldBe(str20.repeat(1), "12345678901234567890");
    shouldBe(str20.repeat(5).length, 100);

    let str4 = "abcd";
    shouldBe(str4.repeat(256).length, 1024);
    shouldBe(str4.repeat(257).length, 1028);

    let utf16_8 = "\u0100\u0101\u0102\u0103\u0104\u0105\u0106\u0107";
    shouldBe(utf16_8.repeat(1).length, 8);
    shouldBe(utf16_8.repeat(128).length, 1024);
    shouldBe(utf16_8.repeat(129).length, 1032);

    let utf16_2 = "\u0100\u0101";
    shouldBe(utf16_2.repeat(512).length, 1024);
    shouldBe(utf16_2.repeat(513).length, 1026);
}

{
    shouldBe("ab".repeat(2.0), "abab");
    shouldBe("ab".repeat(2.1), "abab");
    shouldBe("ab".repeat(2.5), "abab");
    shouldBe("ab".repeat(2.9), "abab");
    shouldBe("ab".repeat(2.999999), "abab");
    shouldBe("ab".repeat(0.1), "");
    shouldBe("ab".repeat(0.5), "");
    shouldBe("ab".repeat(0.9), "");
    shouldBe("ab".repeat(0.999999), "");
    shouldBe("ab".repeat(-0.0), "");
    shouldBe("ab".repeat(-0.1), "");
    shouldBe("ab".repeat(-0.9), "");
    shouldThrow(() => "ab".repeat(-1.0), RangeError);
    shouldThrow(() => "ab".repeat(-1.5), RangeError);
}

{
    shouldBe("x".repeat({ valueOf() { return 3; } }), "xxx");
    shouldBe("x".repeat({ valueOf() { return 2.7; } }), "xx");
    shouldBe("x".repeat({ valueOf() { return 0; } }), "");
    shouldBe("x".repeat({ valueOf() { return "4"; } }), "xxxx");
    shouldBe("x".repeat({ toString() { return "2"; } }), "xx");
    shouldBe("x".repeat({
        valueOf() { return 3; },
        toString() { return 5; }
    }), "xxx");
    shouldThrow(() => {
        "x".repeat({ valueOf() { throw new TypeError("custom"); } });
    }, TypeError);
    shouldBe("ab".repeat(null), "");
    shouldBe("ab".repeat(undefined), "");
    shouldBe("ab".repeat(false), "");
    shouldBe("ab".repeat(true), "ab");
    shouldBe("ab".repeat(NaN), "");
}

{
    shouldThrow(() => String.prototype.repeat.call(null, 1), TypeError);
    shouldThrow(() => String.prototype.repeat.call(undefined, 1), TypeError);
    shouldThrow(() => String.prototype.repeat.call(Symbol("test"), 1), TypeError);
}

{
    let obj = { toString() { return "abc"; } };
    shouldBe(String.prototype.repeat.call(obj, 2), "abcabc");
    shouldBe(String.prototype.repeat.call(obj, 0), "");

    let obj2 = {
        toString() { return "XY"; },
        valueOf() { return "ZZ"; }
    };
    shouldBe(String.prototype.repeat.call(obj2, 3), "XYXYXY");

    shouldBe(String.prototype.repeat.call(123, 2), "123123");
    shouldBe(String.prototype.repeat.call(3.14, 2), "3.143.14");
    shouldBe(String.prototype.repeat.call(true, 2), "truetrue");
    shouldBe(String.prototype.repeat.call(false, 3), "falsefalsefalse");
    shouldBe(String.prototype.repeat.call([1, 2, 3], 2), "1,2,31,2,3");
    shouldBe(String.prototype.repeat.call([], 5), "");

    shouldThrow(() => {
        String.prototype.repeat.call({ toString() { throw new RangeError("custom"); } }, 1);
    }, RangeError);
}

{
    shouldThrow(() => "ab".repeat(0x7FFFFFFF), Error);
    shouldThrow(() => "ab".repeat(0xFFFFFFFF), Error);

    let longStr = "x".repeat(10000);
    shouldThrow(() => longStr.repeat(0x7FFFFFFF), Error);
}

{
    shouldThrow(() => "a".repeat(-1), RangeError);
    shouldBe("a".repeat(-0.0001), "a".repeat(0));
    shouldThrow(() => "a".repeat(-1e10), RangeError);
    shouldThrow(() => "a".repeat(-Infinity), RangeError);
    shouldThrow(() => "a".repeat(Infinity), RangeError);
    shouldThrow(() => "abc".repeat(Infinity), RangeError);
    shouldThrow(() => "a".repeat(1e100), Error);
}

{
    let result = "ABC".repeat(100);
    shouldBe(result.length, 300);
    for (let i = 0; i < 100; i++) {
        shouldBe(result.substring(i * 3, i * 3 + 3), "ABC");
    }

    let result2 = "XY".repeat(7);
    shouldBe(result2, "XYXYXYXYXYXYXY");

    let result3 = "123".repeat(11);
    shouldBe(result3.length, 33);
    shouldBe(result3, "123123123123123123123123123123123");

    let result4 = "\u0100\u0101".repeat(5);
    shouldBe(result4, "\u0100\u0101\u0100\u0101\u0100\u0101\u0100\u0101\u0100\u0101");
}

{
    let ropeStr = "abcdefghi";
    shouldBe(ropeStr.repeat(2).length, 18);
    shouldBe(ropeStr.repeat(3).length, 27);
    shouldBe(ropeStr.repeat(4).length, 36);
    shouldBe(ropeStr.repeat(5).length, 45);
    shouldBe(ropeStr.repeat(7).length, 63);
    shouldBe(ropeStr.repeat(8).length, 72);
    shouldBe(ropeStr.repeat(15).length, 135);
    shouldBe(ropeStr.repeat(16).length, 144);
    shouldBe(ropeStr.repeat(31).length, 279);
    shouldBe(ropeStr.repeat(32).length, 288);
    shouldBe(ropeStr.repeat(63).length, 567);
    shouldBe(ropeStr.repeat(64).length, 576);

    for (let i = 2; i <= 100; i++) {
        let result = ropeStr.repeat(i);
        shouldBe(result.length, 9 * i);
        for (let j = 0; j < i; j++) {
            shouldBe(result.substring(j * 9, j * 9 + 9), ropeStr);
        }
    }
}

{
    function test(str, count) {
        return str.repeat(count);
    }
    noInline(test);

    for (let i = 0; i < testLoopCount; i++) {
        shouldBe(test("a", 5), "aaaaa");
    }

    shouldBe(test("", 100), "");
    shouldBe(test("xyz", 0), "");
    shouldBe(test("ab", 1), "ab");

    for (let i = 0; i < testLoopCount; i++) {
        shouldBe(test("x", i % 20), "x".repeat(i % 20));
        shouldBe(test("abcdefgh", i % 10), "abcdefgh".repeat(i % 10));
        shouldBe(test("abcdefghi", i % 10), "abcdefghi".repeat(i % 10));
    }
}

{
    let mixed = "abc\u0100def";
    shouldBe(mixed.repeat(2), "abc\u0100defabc\u0100def");
    shouldBe(mixed.repeat(3).length, 21);

    let latin1 = "abc";
    let utf16 = "abc\u0100";

    for (let i = 1; i <= testLoopCount; i++) {
        shouldBe(latin1.repeat(i).length, 3 * i);
        shouldBe(utf16.repeat(i).length, 4 * i);
    }
}
