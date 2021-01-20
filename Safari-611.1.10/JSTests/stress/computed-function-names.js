let nullSymbol = Symbol();

let propKeys = [
     "foo", "", undefined, null, true, false, 0, 10, 1234.567,
     Symbol("foo"), Symbol(""), nullSymbol,
];

function toKeyString(x) {
    if (typeof x === "string")
        return '"' + x + '"';
    if (typeof x === "symbol")
        return x.toString();
    return "" + x;
}

function toFuncName(x) {
    if (typeof x === "symbol") {
        if (x !== nullSymbol) {
            let str = x.toString();
            let key = str.slice(7, str.length - 1);
            return "[" + key + "]";
        }
        return "";
    }
    return "" + x;
}

function shouldBe(title, actual, expected) {
    if (actual !== expected)
        throw Error(title + ": actual:" + actual + " expected:" + expected);
}

function makeObj(propKey, classMethodName) {
    return {
        [propKey]: class { static [classMethodName](){} },
    };
}
noInline(makeObj);

for (var i = 0; i < 1000; i++) {
    for (var k = 0; k < propKeys.length; k++) {
        let key = propKeys[k];
        let o = makeObj(key, "prop");
        shouldBe("typeof o[" + toKeyString(key) + "].name", typeof o[key].name, "string");
        shouldBe("o[" + toKeyString(key) + "].name", o[key].name, toFuncName(key));
    }

    for (var k = 0; k < propKeys.length; k++) {
        let key = propKeys[k];
        let o = makeObj(key, "name");
        shouldBe("typeof o[" + toKeyString(key) + "].name", typeof o[key], "function");
    }

    for (var k = 0; k < propKeys.length; k++) {
        let key = propKeys[k];
        let prop = { toString() { return "prop" } };
        let o = makeObj(key, prop);
        shouldBe("typeof o[" + toKeyString(key) + "].name", typeof o[key].name, "string");
        shouldBe("o[" + toKeyString(key) + "].name", o[key].name, toFuncName(key));
    }

    for (var k = 0; k < propKeys.length; k++) {
        let key = propKeys[k];
        let prop = { toString() { return "name" } };
        let o = makeObj(key, prop);
        shouldBe("typeof o[" + toKeyString(key) + "].name", typeof o[key], "function");
    }
}
