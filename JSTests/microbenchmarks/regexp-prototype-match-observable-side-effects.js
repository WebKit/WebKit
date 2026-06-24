//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ runNoFTL

function assert(testedValue, msg) {
    if (!testedValue)
        throw Error(msg);
}

// RegExp subclass should not be able to override lastIndex via accessors
// in the fast path; the C++ fast path reads it directly.
(function () {
    let accesses = [];
    class SubRegExp extends RegExp {
        get lastIndex() {
            accesses.push("getLastIndex");
            return super.lastIndex;
        }
        set lastIndex(newIndex) {
            accesses.push("setLastIndex");
            super.lastIndex = newIndex;
        }
    }

    let obj = new SubRegExp(/rch/);

    assert(accesses == "", "Should not be able to override lastIndex");
    let result = RegExp.prototype[Symbol.match].call(obj, "searchme");
    assert(accesses == "", "Should not be able to override lastIndex");
    assert(result[0] === "rch", "Unexpected result");
})();

// RegExp subclass overriding exec should be observed (slow path).
(function () {
    let accesses = [];
    class SubRegExp extends RegExp {
        exec(str) {
            accesses.push("exec");
            return super.exec(str);
        }
    }

    let obj = new SubRegExp(/rch/);

    assert(accesses == "", "unexpected call to overridden props");
    let result = RegExp.prototype[Symbol.match].call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result[0] === "rch", "Unexpected result");
})();

// 2 levels of RegExp subclasses with the middle parent overriding exec.
(function () {
    let accesses = [];
    class RegExpB extends RegExp {
        exec(str) {
            accesses.push("exec");
            return super.exec(str);
        }
    }
    class RegExpC extends RegExpB { }

    assert(RegExpB.__proto__ == RegExp);
    assert(RegExpC.__proto__ == RegExpB);

    let obj = new RegExpC(/rch/);

    assert(accesses == "", "unexpected call to overridden props");
    let result = RegExp.prototype[Symbol.match].call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result[0] === "rch", "Unexpected result");
})();

// RegExp subclass overriding flags is observed (the slow path reads `flags`).
(function () {
    let accesses = [];
    class SubRegExp extends RegExp {
        get flags() {
            accesses.push("flags");
            return super.flags;
        }
    }

    let obj = new SubRegExp(/rch/, "g");

    assert(accesses == "", "unexpected call to overridden props");
    let result = RegExp.prototype[Symbol.match].call(obj, "rch_rch");
    assert(accesses == "flags", "Property accesses do not match expectation: " + accesses);
    assert(JSON.stringify(result) === JSON.stringify(["rch", "rch"]), "Unexpected result: " + result);
})();

// Proxied RegExp observing every get on a non-global regex.
(function () {
    let accesses = [];
    let regexp = new RegExp(/rch/);
    let proxy = new Proxy(regexp, {
        get(obj, prop) {
            accesses.push(prop.toString());
            if (prop == "exec") {
                return function(str) {
                    return obj.exec(str);
                }
            }
            return obj[prop];
        }
    });

    assert(accesses == "", "unexpected call to overridden props");
    let result = RegExp.prototype[Symbol.match].call(proxy, "searchme");
    // Non-global path: just calls flags getter and exec, no lastIndex access.
    assert(accesses.toString() == "flags,exec", "Proxy not able to observe some gets, got: " + accesses);
    assert(result[0] === "rch", "Unexpected result");
})();

// Proxied global RegExp: global path resets lastIndex to 0 then loops exec
// until it returns null.
(function () {
    let accesses = [];
    let regexp = /rch/g;
    let proxy = new Proxy(regexp, {
        get(obj, prop) {
            accesses.push("get_" + prop.toString());
            if (prop == "exec") {
                return function(str) {
                    return obj.exec(str);
                }
            }
            return obj[prop];
        },
        set(obj, prop, value) {
            accesses.push("set_" + prop.toString() + ":" + value);
            obj[prop] = value;
            return true;
        }
    });

    let result = RegExp.prototype[Symbol.match].call(proxy, "rch_rch");
    assert(JSON.stringify(result) === JSON.stringify(["rch", "rch"]), "Unexpected result: " + result);
    // flags read once; lastIndex set to 0 once; exec called 3 times (twice
    // returning a match, once returning null).
    assert(accesses[0] === "get_flags", "Expected first access to be flags, got: " + accesses[0]);
    assert(accesses.includes("set_lastIndex:0"), "Expected lastIndex to be reset");
})();
