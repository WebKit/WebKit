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
    shouldBe("function", typeof String.prototype.padEnd);
    shouldBe(Object.getPrototypeOf(Function), Object.getPrototypeOf(String.prototype.padEnd));
    shouldBe("padEnd", String.prototype.padEnd.name);

    var descriptor = Object.getOwnPropertyDescriptor(String.prototype, "padEnd");
    shouldBe(undefined, descriptor.get);
    shouldBe(undefined, descriptor.set);
    shouldBe(true, descriptor.configurable);
    shouldBe(false, descriptor.enumerable);
    shouldBe(true, descriptor.writable);
    shouldBe(String.prototype.padEnd, descriptor.value);

    shouldThrow(() => new Function(`${String.prototype.padEnd}`), SyntaxError);
})();

(function TestRequireObjectCoercible() {
    var padEnd = String.prototype.padEnd;
    shouldThrow(() => padEnd.call(null, 4, "test"), TypeError);
    shouldThrow(() => padEnd.call(undefined, 4, "test"), TypeError);
    shouldBe("123   ", padEnd.call({
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
    shouldBe("6.7   ", padEnd.call(proxy, 6, " "));

    proxy = new Proxy({}, {
        get(t, name) {
            if (name === Symbol.toPrimitive || name === "valueOf") return;
            if (name === "toString") return () => 6.7;
            throw new Error("unreachable code");
        }
    });
    shouldBe("6.7   ", padEnd.call(proxy, 6, " "));
})();

(function TestToLength() {
    shouldThrow(() => "123".padEnd(Symbol("16")), TypeError);
    shouldBe("123", "123".padEnd(-1));
    shouldBe("123", "123".padEnd({ toString() { return -1; } }));
    shouldBe("123", "123".padEnd(-0));
    shouldBe("123", "123".padEnd({ toString() { return -0; } }));
    shouldBe("123", "123".padEnd(+0));
    shouldBe("123", "123".padEnd({ toString() { return +0; } }));
    shouldBe("123", "123".padEnd(NaN));
    shouldBe("123", "123".padEnd({ toString() { return NaN; } }));
})();

(function TestFillerToString() {
    shouldBe(".         ", ".".padEnd(10));
    shouldBe(".         ", ".".padEnd(10, undefined));
    shouldBe(".         ", ".".padEnd(10, { toString() { return ""; } }));
    shouldBe(".nullnulln", ".".padEnd(10, null));
})();

(function TestSingleCharacterFiller() {
    shouldBe(".!!!!!!!!!", ".".padEnd(10, "!"));
    shouldBe(".!!!!!!!!!", ".".padEnd(10, { toString() { return "!"; } }));
    shouldBe(".!!!!!!!!!", ".".padEnd(10, "" + "!" + ""));
    shouldBe(".!!!!!!!!!", ".".padEnd(10, { toString() { return "" + "!" + ""; } }));
})();

(function TestMemoryLimits() {
    shouldThrow(() => ".".padEnd(0x80000000, "o"), Error);
    shouldThrow(() => ".".padEnd({ valueOf() { return 0x80000000; } }, "o"), Error);
    shouldThrow(() => ".".padEnd("0x80000000", "o"), Error);
})();

(function TestFillerRepetition() {
    for (var i = 2000; i > 0; --i) {
        var expected = "123" + "xoxo".repeat(i / 4).slice(0, i - 3);
        var actual = "123".padEnd(i, "xoxo");
        shouldBe(expected, actual);
        shouldBe(i > "123".length ? i : 3, actual.length);
        actual = "123".padEnd(i, "xo" + "" + "xo");
        shouldBe(expected, actual);
        shouldBe(i > "123".length ? i : 3, actual.length);
    }
})();
