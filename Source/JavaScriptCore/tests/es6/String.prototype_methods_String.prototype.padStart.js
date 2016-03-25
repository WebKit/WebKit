function shouldBe(expected, actual, msg) {
    if (msg === void 0)
        msg = '';
    else
        msg = ' for ' + msg;
    if (actual !== expected)
        throw new Error('bad value' + msg + ': ' + actual + '. Expected ' + expected);
}

function shouldThrow(func, errorType) {
    try {
        func();
        throw new Error('Expected ' + func + '() to throw ' + errorType.name + ', but did not throw.');
    } catch (e) {
        if (e instanceof errorType) return;
        throw new Error('Expected ' + func + '() to throw ' + errorType.name + ', but threw ' + e);
    }
}

(function TestMeta() {
    shouldBe(1, String.prototype.padEnd.length);
    shouldBe("function", typeof String.prototype.padStart);
    shouldBe(Object.getPrototypeOf(Function), Object.getPrototypeOf(String.prototype.padStart));
    shouldBe("padStart", String.prototype.padStart.name);

    var descriptor = Object.getOwnPropertyDescriptor(String.prototype, "padStart");
    shouldBe(undefined, descriptor.get);
    shouldBe(undefined, descriptor.set);
    shouldBe(true, descriptor.configurable);
    shouldBe(false, descriptor.enumerable);
    shouldBe(true, descriptor.writable);
    shouldBe(String.prototype.padStart, descriptor.value);

    shouldThrow(() => new Function(`${String.prototype.padStart}`), SyntaxError);
})();

(function TestRequireObjectCoercible() {
    var padStart = String.prototype.padStart;
    shouldThrow(() => padStart.call(null, 4, "test"), TypeError);
    shouldThrow(() => padStart.call(undefined, 4, "test"), TypeError);
    shouldBe("   123", padStart.call({
        __proto__: null,
        valueOf() { return 123; }
    }, 6, " "));

    var proxy = new Proxy({}, {
        get(t, name) {
            if (name === Symbol.toPrimitive || name === "toString") return;
            if (name === "valueOf") return () => 6.7;
            throw new Error("unreachable code");
        }
    });
    shouldBe("   6.7", padStart.call(proxy, 6, " "));

    proxy = new Proxy({}, {
        get(t, name) {
            if (name === Symbol.toPrimitive || name === "valueOf") return;
            if (name === "toString") return () => 6.7;
            throw new Error("unreachable code");
        }
    });
    shouldBe("   6.7", padStart.call(proxy, 6, " "));
})();

(function TestToLength() {
    shouldThrow(() => "123".padStart(Symbol("16")), TypeError);
    shouldBe("123", "123".padStart(-1));
    shouldBe("123", "123".padStart({ toString() { return -1; } }));
    shouldBe("123", "123".padStart(-0));
    shouldBe("123", "123".padStart({ toString() { return -0; } }));
    shouldBe("123", "123".padStart(+0));
    shouldBe("123", "123".padStart({ toString() { return +0; } }));
    shouldBe("123", "123".padStart(NaN));
    shouldBe("123", "123".padStart({ toString() { return NaN; } }));
})();

(function TestFillerToString() {
    shouldBe("         .", ".".padStart(10));
    shouldBe("         .", ".".padStart(10, undefined));
    shouldBe("         .", ".".padStart(10, { toString() { return ""; } }));
    shouldBe("nullnulln.", ".".padStart(10, null));
})();

(function TestSingleCharacterFiller() {
    shouldBe("!!!!!!!!!.", ".".padStart(10, "!"));
    shouldBe("!!!!!!!!!.", ".".padStart(10, { toString() { return "!"; } }));
    shouldBe("!!!!!!!!!.", ".".padStart(10, "" + "!" + ""));
    shouldBe("!!!!!!!!!.", ".".padStart(10, { toString() { return "" + "!" + ""; } }));
})();

(function TestMemoryLimits() {
    shouldThrow(() => ".".padStart(0x80000000, "o"), Error);
    shouldThrow(() => ".".padStart({ valueOf() { return 0x80000000; } }, "o"), Error);
    shouldThrow(() => ".".padStart("0x80000000", "o"), Error);
})();

(function TestFillerRepetition() {
    for (var i = 2000; i > 0; --i) {
        var expected = "xoxo".repeat(i / 4).slice(0, i - 3) + "123";
        var actual = "123".padStart(i, "xoxo");
        shouldBe(expected, actual);
        shouldBe(i > "123".length ? i : 3, actual.length);
        actual = "123".padStart(i, "xo" + "" + "xo");
        shouldBe(expected, actual);
        shouldBe(i > "123".length ? i : 3, actual.length);
    }
})();
