function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

{
    let error = null;
    let target = { };
    let handler = {
        ownKeys: function() {
            error = new Error;
            throw error;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.keys(proxy);
        } catch(e) {
            threw = true;
            assert(e === error);
        }
        assert(threw);
    }
}

{
    let error = null;
    let target = { };
    let handler = {
        get ownKeys() {
            error = new Error;
            throw error;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.keys(proxy);
        } catch(e) {
            threw = true;
            assert(e === error);
        }
        assert(threw);
    }
}

{
    let target = {
        x: 40
    };
    let called = false;
    let handler = {
        ownKeys: function(theTarget) {
            called = true;
            return ["1", 2, 3];
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.keys(proxy);
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy handler's 'ownKeys' method must return an array-like object containing only Strings and Symbols");
        }
        assert(threw);
        assert(called);
        called = false;
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", {
        configurable: false,
        enumerable: true,
        value: 400
    });
    let called = false;
    let handler = {
        ownKeys: function(theTarget) {
            called = true;
            return [];
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.keys(proxy);
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy object's 'target' has the non-configurable property 'x' that was not in the result from the 'ownKeys' trap");
        }
        assert(threw);
        assert(called);
        called = false;
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", {
        configurable: true,
        enumerable: true,
        value: 400
    });
    Object.preventExtensions(target);
    let called = false;
    let handler = {
        ownKeys: function(theTarget) {
            called = true;
            return [];
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.keys(proxy);
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy object's non-extensible 'target' has configurable property 'x' that was not in the result from the 'ownKeys' trap");
        }
        assert(threw);
        assert(called);
        called = false;
    }

    for (let i = 0; i < 500; i++) {
        let threw = false;
        let foundKey = false;
        try {
            for (let k in proxy)
                foundKey = true;
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy object's non-extensible 'target' has configurable property 'x' that was not in the result from the 'ownKeys' trap");
            assert(!foundKey);
        }
        assert(threw);
        assert(called);
        called = false;
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", {
        configurable: true,
        enumerable: true,
        value: 400
    });
    Object.preventExtensions(target);
    let called = false;
    let handler = {
        ownKeys: function(theTarget) {
            called = true;
            return ["x", "y"];
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.keys(proxy);
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy handler's 'ownKeys' method returned a key that was not present in its non-extensible target");
        }
        assert(threw);
        assert(called);
        called = false;
    }

    for (let i = 0; i < 500; i++) {
        let threw = false;
        let reached = false;
        try {
            for (let k in proxy)
                reached = true;
        } catch (e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy handler's 'ownKeys' method returned a key that was not present in its non-extensible target");
        }
        assert(threw);
        assert(called);
        assert(!reached);
        called = false;
    }
}

{
    let target = {};
    let called1 = false;
    let called2 = false;
    Object.defineProperty(target, 'a', { value: 42, configurable: false });
    let p1 = new Proxy(target, {
        ownKeys() {
            called1 = true;
            return ['a', 'a'];
        }
    });
    let p2 = new Proxy(p1, {
        ownKeys() {
            called2 = true;
            return ['a'];
        }
    });

    for (let i = 0; i < 500; i++) {
        // Throws per https://github.com/tc39/ecma262/pull/833
        let threw = false;
        try {
            Reflect.ownKeys(p2);
        } catch(e) {
            assert(e.toString() === "TypeError: Proxy handler's 'ownKeys' trap result must not contain any duplicate names");
            threw = true;
        }
        assert(threw);
        assert(called1);
        assert(called2);
    }
}

{
    let target = {};
    let called1 = false;
    let called2 = false;
    Object.defineProperty(target, 'a', { value: 42, configurable: true });
    Object.preventExtensions(target);
    let p1 = new Proxy(target, {
        ownKeys() {
            called1 = true;
            return ['a', 'a'];
        }
    });
    let p2 = new Proxy(p1, {
        ownKeys() {
            called2 = true;
            return ['a'];
        }
    });

    for (let i = 0; i < 500; i++) {
        // Throws per https://github.com/tc39/ecma262/pull/833
        let threw = false;
        try {
            Reflect.ownKeys(p2);
        } catch(e) {
            assert(e.toString() === "TypeError: Proxy handler's 'ownKeys' trap result must not contain any duplicate names");
            threw = true;
        }
        assert(threw);
        assert(called1);
        assert(called2);
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", {
        configurable: true,
        enumerable: true,
        value: 400
    });
    Object.preventExtensions(target);
    let called = false;
    let handler = {
        ownKeys: function(theTarget) {
            called = true;
            return ["x", "x"];
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        try {
            Object.keys(proxy);
        } catch(e) {
            assert(e.toString() === "TypeError: Proxy handler's 'ownKeys' trap result must not contain any duplicate names");
            threw = true;
        }
        assert(called);
        assert(threw);
        called = false;
        threw = false;
    }
}

{
    let target = { };
    let handler = {
        ownKeys: 45
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.keys(proxy);
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: 'ownKeys' property of a Proxy's handler should be callable");
        }
        assert(threw);
    }
}

function shallowEq(a, b) {
    if (a.length !== b.length)
        return false;
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            return false;
    }

    return true;
}

{
    let target = {
        x: 40
    };
    let called = false;
    let arr = ["a", "b", "c"];
    let handler = {
        ownKeys: function(theTarget) {
            called = true;
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Object.keys(proxy);
        assert(result !== arr);
        assert(shallowEq(result, []));
        assert(called);
        called = false;
    }
}

{
    let target = {
        x: 40
    };
    let called = false;
    let arr = ["a", "b", "c"];
    let handler = {
        getOwnPropertyDescriptor: function(theTarget, propertyName) {
            if (arr.indexOf(propertyName) >= 0) {
                return {
                    enumerable: true,
                    configurable: true
                };
            }
            return Reflect.getOwnPropertyDescriptor(theTarget, propertyName);
        },

        ownKeys: function(theTarget) {
            called = true;
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Object.keys(proxy);
        assert(result !== arr);
        assert(shallowEq(result, arr));
        assert(called);
        called = false;
    }
}

{
    let target = {
        x: 40
    };
    let called = false;
    let arr = ["a", "b", "c"];
    let handler = {
        ownKeys: function(theTarget) {
            called = true;
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.ownKeys(proxy);
        assert(result !== arr);
        assert(shallowEq(result, arr));
        assert(called);
        called = false;
    }
}

{
    let target = {
        x: 40
    };
    let called = false;
    let s1 = Symbol();
    let s2 = Symbol();
    let arr = ["a", "b", s1, "c", s2];
    let handler = {
        ownKeys: function(theTarget) {
            called = true;
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Object.getOwnPropertySymbols(proxy);
        assert(shallowEq(result, [s1, s2]));
        assert(called);
        called = false;
    }
}

{
    let target = {
        x: 40
    };
    let called = false;
    let s1 = Symbol();
    let s2 = Symbol();
    let arr = ["a", "b", s1, "c", s2];
    let handler = {
        getOwnPropertyDescriptor(theTarget, propertyName) {
            if (arr.indexOf(propertyName) >= 0) {
                return {
                    enumerable: true,
                    configurable: true
                }
            }
            return Reflect.getOwnPropertyDescriptor(theTarget, propertyName);
        },
        ownKeys: function(theTarget) {
            called = true;
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Object.keys(proxy);
        assert(shallowEq(result, ["a", "b", "c"]));
        assert(called);
        called = false;
    }
}

{
    let target = {
        x: 40
    };
    let called = false;
    let s1 = Symbol();
    let s2 = Symbol();
    let arr = ["a", "b", s1, "c", s2];
    let handler = {
        ownKeys: function(theTarget) {
            called = true;
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.ownKeys(proxy);
        assert(shallowEq(result, arr));
        assert(called);
        called = false;
    }
}

{
    let target = {
        x: 40
    };
    let called = false;
    let s1 = Symbol();
    let s2 = Symbol();
    let arr = ["a", "b", s1, "c", s2];
    let handler = {
        getOwnPropertyDescriptor: () => {
            return { enumerable: true, configurable: true }
        },
        ownKeys: function(theTarget) {
            called = true;
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let set = new Set;
        for (let p in proxy)
            set.add(p);
        assert(set.size === 3);
        assert(set.has("a"));
        assert(set.has("b"));
        assert(set.has("c"));
        assert(called);
        called = false;
    }
}

{
    let target = {
        x: 40
    };
    let called = false;
    let s1 = Symbol();
    let s2 = Symbol();
    let arr = ["a", "b", s1, "c", s2];
    let handler = {
        getOwnPropertyDescriptor: () => {
            return { enumerable: true, configurable: true }
        },
        ownKeys: function(theTarget) {
            called = true;
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let set = new Set;
        for (let p in proxy)
            set.add(p);
        if (i === 40) { // Make sure we don't cache the result.
            arr.push("d");
        }
        assert(set.size === i > 40 ? 4 : 3);
        assert(set.has("a"));
        assert(set.has("b"));
        assert(set.has("c"));
        if (i > 40)
            assert(set.has("d"));
        assert(called);
        called = false;
    }
}

{
    let target = {
        x: 40
    };
    let called = false;
    let s1 = Symbol();
    let s2 = Symbol();
    let arr = ["a", "b", s1, "c", s2];
    let handler = {
        getOwnPropertyDescriptor: () => {
            return { enumerable: true, configurable: true }
        },
        ownKeys: function(theTarget) {
            called = true;
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    let proxyish = Object.create(proxy, {
        d: { enumerable: true, configurable: true }
    });
    for (let i = 0; i < 500; i++) {
        let set = new Set;
        for (let p in proxyish)
            set.add(p);
        assert(set.size === 4);
        assert(set.has("a"));
        assert(set.has("b"));
        assert(set.has("c"));
        assert(set.has("d"));
        assert(called);
        called = false;
    }
}

{
    let target = {
        x: 40
    };
    let called = false;
    let s1 = Symbol();
    let s2 = Symbol();
    let arr = ["a", "b", s1, "c", s2];
    let handler = {
        getOwnPropertyDescriptor: () => {
            return { enumerable: true, configurable: true }
        },
        ownKeys: function(theTarget) {
            called = true;
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    let proxyish = Object.create(proxy, {
        d: { enumerable: true, configurable: true }
    });
    for (let i = 0; i < 500; i++) {
        let set = new Set;
        for (let p in proxyish)
            set.add(p);
        assert(set.size === 4);
        assert(set.has("a"));
        assert(set.has("b"));
        assert(set.has("c"));
        assert(set.has("d"));
        assert(called);
        called = false;
    }
}

{
    let called = false;
    let target = {x: 20, y: 40};
    let handler = {
        ownKeys: null
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let keys = Object.keys(proxy);
        assert(keys.indexOf("x") !== -1);
        assert(keys.indexOf("y") !== -1);
    }
}

{
    let called = false;
    let target = new Proxy({}, {
        ownKeys: function(theTarget) {
            called = true;
            return Reflect.ownKeys(theTarget);
        }
    });
    let s1 = Symbol();
    let s2 = Symbol();
    let arr = ["a", "b", s1, "c", s2];
    let handler = {
        ownKeys: function(theTarget) {
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let keys = Object.keys(proxy);
        assert(called);
        called = false;
    }
}

{
    let error = null;
    let target = new Proxy({}, {
        ownKeys: function(theTarget) {
            error = new Error;
            throw error;
        }
    });
    let s1 = Symbol();
    let s2 = Symbol();
    let arr = ["a", "b", s1, "c", s2];
    let handler = {
        ownKeys: function(theTarget) {
            return arr;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.keys(proxy);
        } catch(e) {
            threw = true;
            assert(e === error);
        }
        assert(threw);
        error = null;
    }
}

{
    let error = null;
    let s1 = Symbol();
    let s2 = Symbol();
    let target = Object.defineProperties({}, {
        x: {
            value: "X",
            enumerable: true,
            configurable: true,
        },
        dontEnum1: {
            value: "dont-enum",
            enumerable: false,
            configurable: true,
        },
        y: {
            get() { return "Y"; },
            enumerable: true,
            configurable: true,
        },
        dontEnum2: {
            get() { return "dont-enum-accessor" },
            enumerable: false,
            configurable: true
        },
        [s1]: {
            value: "s1",
            enumerable: true,
            configurable: true,
        },
        [s2]: {
            value: "dont-enum-symbol",
            enumerable: false,
            configurable: true,  
        },
    });
    let checkedOwnKeys = false;
    let checkedPropertyDescriptor = false;
    let handler = {
        ownKeys() {
            checkedOwnKeys = true;
            return ["x", "dontEnum1", "y", "dontEnum2", s1, s2];
        },
        getOwnPropertyDescriptor(t, k) {
            checkedPropertyDescriptors = true;
            return Reflect.getOwnPropertyDescriptor(t, k);
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        checkedPropertyDescriptors = false;
        assert(shallowEq(["x", "dontEnum1", "y", "dontEnum2", s1, s2], Reflect.ownKeys(proxy)));
        assert(checkedOwnKeys);
        assert(!checkedPropertyDescriptors);
        checkedOwnKeys = false;

        let enumerableStringKeys = [];
        for (let k in proxy)
            enumerableStringKeys.push(k);
        assert(shallowEq(["x", "y"], enumerableStringKeys));
        assert(checkedOwnKeys);
        assert(checkedPropertyDescriptors);
    }
}
