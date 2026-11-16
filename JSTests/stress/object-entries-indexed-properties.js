function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function check(object, expected) {
    var entries = Object.entries(object);
    shouldBe(Array.isArray(entries), true);
    shouldBe(entries.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        var entry = entries[i];
        shouldBe(Array.isArray(entry), true);
        shouldBe(entry.length, 2);
        shouldBe(typeof entry[0], "string");
        shouldBe(entry[0], expected[i][0]);
        shouldBe(Object.is(entry[1], expected[i][1]), true);
    }
}

function test() {
    check({ 0: 10, 1: 20, 2: 30 }, [["0", 10], ["1", 20], ["2", 30]]);
    check({ 0: "a", 1: "b", 2: "c" }, [["0", "a"], ["1", "b"], ["2", "c"]]);
    check({ 0: 1.5, 1: 2.5, 2: -0.5 }, [["0", 1.5], ["1", 2.5], ["2", -0.5]]);
    check({ 0: 1.5, 1: NaN, 2: -0 }, [["0", 1.5], ["1", NaN], ["2", -0]]);
    check({ 0: undefined, 1: null, 2: false }, [["0", undefined], ["1", null], ["2", false]]);
    check({ x: 1, 1: "b", y: 2, 0: "a" }, [["0", "a"], ["1", "b"], ["x", 1], ["y", 2]]);
    check({ 9: 9, 10: 10, 99: 99, 100: 100, 1023: 1023, 1024: 1024, 1025: 1025 }, [["9", 9], ["10", 10], ["99", 99], ["100", 100], ["1023", 1023], ["1024", 1024], ["1025", 1025]]);

    {
        let object = {};
        object[7] = 7;
        object[2] = 2;
        object.n = "named";
        object[5] = 5;
        check(object, [["2", 2], ["5", 5], ["7", 7], ["n", "named"]]);
        delete object[5];
        check(object, [["2", 2], ["7", 7], ["n", "named"]]);
        delete object[7];
        delete object[2];
        check(object, [["n", "named"]]);
    }

    {
        let object = {};
        object[3] = 0.5;
        object[1] = 1.5;
        check(object, [["1", 1.5], ["3", 0.5]]);
        object[2] = "string";
        check(object, [["1", 1.5], ["2", "string"], ["3", 0.5]]);
    }

    {
        let object = {};
        object[100000] = "big";
        object[5] = "small";
        object[50000] = "mid";
        object.k = "named";
        check(object, [["5", "small"], ["50000", "mid"], ["100000", "big"], ["k", "named"]]);
    }

    {
        let object = {};
        object[4294967295] = "not an index";
        object[4294967294] = "max";
        object[2147483648] = "above int32";
        object[2147483647] = "int32 max";
        object[0] = "zero";
        check(object, [["0", "zero"], ["2147483647", "int32 max"], ["2147483648", "above int32"], ["4294967294", "max"], ["4294967295", "not an index"]]);
    }

    {
        let object = { 0: "a", 10: "visible", k: "named" };
        Object.defineProperty(object, 20, { value: "hidden", enumerable: false });
        Object.defineProperty(object, 5, { value: "read only", enumerable: true, writable: false });
        check(object, [["0", "a"], ["5", "read only"], ["10", "visible"], ["k", "named"]]);
    }

    {
        let object = { 0: "a", 2: "c", k: "named" };
        Object.defineProperty(object, 1, {
            get() {
                delete object[2];
                object[3] = "added";
                return "getter";
            },
            enumerable: true
        });
        check(object, [["0", "a"], ["1", "getter"], ["k", "named"]]);
    }

    check(Object.freeze({ 1: "b", 0: "a", k: 1 }), [["0", "a"], ["1", "b"], ["k", 1]]);
    check(Object.seal({ 1: 1, 0: 0, k: 1 }), [["0", 0], ["1", 1], ["k", 1]]);
    check(Object.preventExtensions({ 1: 1.5, 0: 0.5, k: 1 }), [["0", 0.5], ["1", 1.5], ["k", 1]]);

    {
        let object = Object.create({ 0: "inherited", 3: "inherited" });
        object[1] = "own";
        check(object, [["1", "own"]]);
    }

    {
        let object = Object.create(null);
        object[1] = "b";
        object[0] = "a";
        check(object, [["0", "a"], ["1", "b"]]);
    }

    {
        class C {
            field = "field";
            constructor() {
                this[1] = "one";
                this[0] = "zero";
            }
        }
        check(new C, [["0", "zero"], ["1", "one"], ["field", "field"]]);
    }

    {
        let object = { 0: "a", [Symbol.iterator]: 1, 1: "b" };
        check(object, [["0", "a"], ["1", "b"]]);
    }

    {
        let object = { 0: "a", 1: "b" };
        let first = Object.entries(object);
        first[0][0] = "changed";
        first[0][1] = "changed";
        first[1].push("pushed");
        first.length = 0;
        check(object, [["0", "a"], ["1", "b"]]);
    }

    check([1, , 3], [["0", 1], ["2", 3]]);
    check("ab", [["0", "a"], ["1", "b"]]);
    check(new Uint8Array([1, 2]), [["0", 1], ["1", 2]]);
    check((function () { return arguments; })("a", "b"), [["0", "a"], ["1", "b"]]);
}

{
    let object = {};
    let expected = [];
    for (let i = 0; i < 10000; ++i) {
        object[i] = i;
        expected.push([String(i), i]);
    }
    object.tail = "end";
    expected.push(["tail", "end"]);
    check(object, expected);
}

for (var i = 0; i < testLoopCount; ++i) {
    var entries = Object.entries({ 0: i, 1: i + 0.5, a: i, 300: "x" });
    shouldBe(entries.length, 4);
    shouldBe(entries[0][0], "0");
    shouldBe(entries[0][1], i);
    shouldBe(entries[1][0], "1");
    shouldBe(entries[1][1], i + 0.5);
    shouldBe(entries[2][0], "300");
    shouldBe(entries[2][1], "x");
    shouldBe(entries[3][0], "a");
    shouldBe(entries[3][1], i);
}

test();
test();

Object.defineProperty(Array.prototype, 0, {
    get() { throw new Error("getter called"); },
    set(value) { throw new Error("setter called"); },
    configurable: true
});
test();
