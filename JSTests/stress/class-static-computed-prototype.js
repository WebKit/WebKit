function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

{
    let log = [];
    function key(name) {
        log.push("key " + name);
        return name;
    }
    try {
        (class {
            static [key("prototype")] = log.push("init prototype");
            static [key("b")] = log.push("init b");
        });
    } catch (e) {
        log.push(e.constructor.name);
    }
    shouldBe(log.join(", "), "key prototype, key b, init prototype, TypeError");
}

{
    let log = [];
    function key(name) {
        log.push("key " + name);
        return name;
    }
    try {
        (class {
            static a = log.push("a");
            static [key("prototype")] = log.push("p");
            static b = log.push("b");
        });
    } catch (e) {
        log.push(e.constructor.name);
    }
    shouldBe(log.join(", "), "key prototype, a, p, TypeError");
}

{
    let log = [];
    function key(name) {
        log.push("key " + name);
        return name;
    }
    let thrown;
    try {
        (class {
            static [key("prototype")] = (() => { throw "init"; })();
            static [key("b")] = log.push("init b");
        });
    } catch (e) {
        thrown = e;
    }
    shouldBe(log.join(", "), "key prototype, key b");
    shouldBe(thrown, "init");
}

{
    let ran = false;
    let error;
    try {
        (class {
            static ["prototype"] = (ran = true);
        });
    } catch (e) {
        error = e;
    }
    shouldBe(ran, true);
    shouldBe(error instanceof TypeError, true);
}

{
    let error;
    try {
        (class {
            static ["prototype"];
        });
    } catch (e) {
        error = e;
    }
    shouldBe(error instanceof TypeError, true);
}

{
    let error;
    try {
        eval("class C { static prototype = 1 }");
    } catch (e) {
        error = e;
    }
    shouldBe(error instanceof SyntaxError, true);
}

{
    let log = [];
    function key(name) {
        log.push("key " + name);
        return name;
    }
    class C {
        [key("prototype")] = (log.push("init"), 1);
    }
    let object = new C();
    shouldBe(log.join(", "), "key prototype, init");
    shouldBe(object.prototype, 1);
    shouldBe(Object.hasOwn(object, "prototype"), true);
}

{
    class C {
        static ["a"] = 1;
        static ["meth"] = function () { };
        static [Symbol.for("prototype")] = 2;
    }
    shouldBe(C.a, 1);
    shouldBe(C.meth.name, "meth");
    shouldBe(C[Symbol.for("prototype")], 2);
}

{
    let log = [];
    try {
        (class {
            static [{ toString() { log.push("key"); return "prototype"; } }] = log.push("init");
            static b = log.push("b");
        });
    } catch (e) {
        log.push(e.constructor.name);
    }
    shouldBe(log.join(", "), "key, init, TypeError");
}
