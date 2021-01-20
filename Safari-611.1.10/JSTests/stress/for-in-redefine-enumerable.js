function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}.`);
}

const enumDesc = { value: 0, writable: true, enumerable: true, configurable: true };
const dontEnumDesc = { value: 0, writable: true, enumerable: false, configurable: true };

// indexed property
(() => {
    function test() {
        var arr = Object.defineProperties([0, 0, 0], { 1: dontEnumDesc });
        for (var i in arr) {
            assert(i in arr);
            shouldBe(arr[i], 0);
            ++arr[i];
            if (i === "0")
                Object.defineProperties(arr, { 1: enumDesc, 2: dontEnumDesc });
        }
        shouldBe(arr[0], 1);
        shouldBe(arr[1], 0);
        shouldBe(arr[2], 0);
    }

    for (var i = 0; i < 1e5; ++i)
        test();
})();

// structure property
(() => {
    function test() {
        var obj = Object.create(null, { a: enumDesc, b: enumDesc, c: dontEnumDesc });
        for (var key in obj) {
            assert(key in obj);
            shouldBe(obj[key], 0);
            ++obj[key];
            if (key === "a")
                Object.defineProperties(obj, { b: dontEnumDesc, c: enumDesc });
        }
        shouldBe(obj.a, 1);
        shouldBe(obj.b, 0);
        shouldBe(obj.c, 0);
    }

    for (var i = 0; i < 1e5; ++i)
        test();
})();

// generic property (Proxy)
(() => {
    function test() {
        var target = { a: 0, b: 0, c: 0 };
        var enumMap = { a: true, b: true, c: false };
        var proxy = new Proxy(target, {
            getOwnPropertyDescriptor: (_, key) => {
                return { value: target[key], writable: true, enumerable: enumMap[key], configurable: true };
            },
        });

        for (var key in proxy) {
            assert(key in proxy);
            shouldBe(proxy[key], 0);
            ++target[key];
            if (key === "a") {
                enumMap.b = false;
                enumMap.c = true;
            }
        }
        shouldBe(target.a, 1);
        shouldBe(target.b, 0);
        shouldBe(target.c, 1);
    }

    for (var i = 0; i < 1e5; ++i)
        test();
})();

// generic property (in prototype)
(() => {
    function test() {
        var seen = {};
        var proto = Object.create(null, { b: enumDesc, c: dontEnumDesc, d: enumDesc, e: enumDesc });
        var heir = Object.create(proto, { a: enumDesc, e: dontEnumDesc });
        for (var key in heir) {
            assert(key in heir);
            shouldBe(heir[key], 0);
            seen[key] = true;
            if (key === "a")
                Object.defineProperties(proto, { b: dontEnumDesc, c: enumDesc });
            if (key === "d")
                Object.defineProperties(heir, { e: enumDesc });
        }
        assert(seen.a);
        assert(!seen.b);
        assert(!seen.c);
        assert(seen.d);
        assert(seen.e);
    }

    for (var i = 0; i < 1e5; ++i)
        test();
})();
