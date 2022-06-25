//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ runNoFTL

function assert(testedValue, msg) {
    if (!testedValue)
        throw Error(msg);
}

//======================================================================================
// Testing the string that we're calling search on.

// Proxied String subclass.
(function () {
    let accesses = [];
    class ExtString extends String { }
    var obj = new ExtString("searchme");
    var proxy = new Proxy(obj, {
        get(obj, prop) {
            accesses.push(prop.toString());
            if (prop === "toString") {
                return function() {
                    accesses.push("in_toString");
                    return obj.toString();
                }
            }
            return obj[prop];
        }
    });

    assert(accesses == "", "unexpected call to overridden props");
    let result = String.prototype.search.call(proxy, "rch");    
    assert(accesses == "Symbol(Symbol.toPrimitive),toString,in_toString", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();

// Object that looks like a string.
(function () {
    let accesses = [];
    var obj = {
        [Symbol.toPrimitive]() {
            accesses.push(Symbol.toPrimitive.toString());
            return "searchme";
        }
    }

    assert(accesses == "", "unexpected call to overridden props");
    let result = String.prototype.search.call(obj, "rch");    
    assert(accesses == "Symbol(Symbol.toPrimitive)", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();

// Object that looks like a string.
(function () {
    let accesses = [];
    var obj = {
        toString() {
            accesses.push("toString");
            return "searchme";
        }
    }

    assert(accesses == "", "unexpected call to overridden props");
    let result = String.prototype.search.call(obj, "rch");    
    assert(accesses == "toString", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();

// String subclass with overridden @@search.
(function () {
    let accesses = [];
    class ExtString extends String {
        [Symbol.search] (str) {
            accesses.push("Symbol(Symbol.search)");
            return /rch/[Symbol.search](str);
        }
    };

    var obj = new ExtString;

    assert(accesses == "", "unexpected call to overridden props");
    let result = "searchme".search(obj);    
    assert(accesses == "Symbol(Symbol.search)", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();


// Object with overridden @@search.
(function () {
    let accesses = [];
    var obj = {
        [Symbol.search] (str) {
            accesses.push("Symbol(Symbol.search)");
            return /rch/[Symbol.search](str);
        },
    }

    assert(accesses == "", "unexpected call to overridden props");
    let result = "searchme".search(obj);
    assert(accesses == "Symbol(Symbol.search)", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();


//======================================================================================
// Testing the regexp object that we're calling search with.

// RegExp subclass should not be able to override lastIndex.
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
    let result = "searchme".search(obj);
    assert(accesses == "", "Should not be able to override lastIndex");
    assert(result === 3, "Unexpected result");
})();

// RegExp subclass overriding exec.
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
    let result = "searchme".search(obj);
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();
 
// Any object with custom prototype overriding lastIndex.
(function () {
    let accesses = [];
    let TestRegExpProto = {
        get lastIndex() {
            accesses.push("getLastIndex");
            return this._regex.lastIndex;
        },
        set lastIndex(newIndex) {
            accesses.push("setLastIndex");
            this._regex.lastIndex = newIndex;
        },
    }
    TestRegExpProto.__proto__ = RegExp.prototype;

    let TestRegExp = function(regex) {
        this._regex = new RegExp(regex);
    }
    TestRegExp.prototype = TestRegExpProto;
    TestRegExpProto.constructor = TestRegExp;

    let obj = new TestRegExp(/rch/);

    assert(accesses == "", "unexpected call to overridden props");
    try {
        let result = "searchme".search(obj);
        assert(false, "Error not thrown");
    } catch (e) {
        assert(e.toString() == "TypeError: Builtin RegExp exec can only be called on a RegExp object",
            "Unexpected error message");
    }
    assert(accesses == "getLastIndex", "Property accesses do not match expectation");
})();

// Any object with custom prototype overriding exec.
(function () {
    let accesses = [];
    let TestRegExpProto = {
        exec(str) {
            accesses.push("exec");
            return this._regex.exec(str);
        }
    }
    TestRegExpProto.__proto__ = RegExp.prototype;

    let TestRegExp = function(regex) {
        this._regex = new RegExp(regex);
    }
    TestRegExp.prototype = TestRegExpProto;
    TestRegExpProto.constructor = TestRegExp;

    let obj = new TestRegExp(/rch/);

    assert(accesses == "", "unexpected call to overridden props");
    let result = "searchme".search(obj);
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
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
    let result = "searchme".search(obj);
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();

// 2 levels of RegExp subclasses with substituted prototype before instantiation.
(function () {
    let accesses = [];

    class B extends RegExp { }
    class C extends B { }

    assert(B.__proto__ === RegExp);
    assert(C.__proto__ === B);
    assert(B.prototype.__proto__ === RegExp.prototype);
    assert(C.prototype.__proto__ === B.prototype);

    let X = function () {}
    Object.defineProperty(X.prototype, "exec", {
        value: function(str) {
            accesses.push("exec");
            return /rch/.exec(str);
        }
    });
    Object.defineProperty(X.prototype, "lastIndex", {
        get: function() {
            accesses.push("getLastIndex");
            return 0;
        },
        set: function(value) {
            accesses.push("setLastIndex");
        }
    });

    // Monkey with the prototype chain before instantiating C.
    X.__proto__ = RegExp;
    X.prototype.__proto__ = RegExp.prototype;
    C.__proto__ = X;
    C.prototype.__proto__ = X.prototype;

    assert(X.__proto__ === RegExp);
    assert(C.__proto__ === X);
    assert(X.prototype.__proto__ === RegExp.prototype);
    assert(C.prototype.__proto__ === X.prototype);

    let obj = new C();

    assert(accesses == "", "unexpected call to overridden props");
    let result = "searchme".search(obj);
    assert(accesses == "getLastIndex,exec,getLastIndex", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();

// 2 levels of RegExp subclasses with substituted prototype after instantiation.
(function () {
    let accesses = [];

    class B extends RegExp { }
    class C extends B { }

    assert(B.__proto__ === RegExp);
    assert(C.__proto__ === B);
    assert(B.prototype.__proto__ === RegExp.prototype);
    assert(C.prototype.__proto__ === B.prototype);

    let obj = new C();

    let X = function () {}
    Object.defineProperty(X.prototype, "exec", {
        value: function(str) {
            accesses.push("exec");
            return /rch/.exec(str);
        }
    });
    Object.defineProperty(X.prototype, "lastIndex", {
        get: function() {
            accesses.push("getLastIndex");
            return 0;
        },
        set: function(value) {
            accesses.push("setLastIndex");
        }
    });

    // Monkey with the prototype chain after instantiating C.
    X.__proto__ = RegExp;
    X.prototype.__proto__ = RegExp.prototype;
    C.__proto__ = X;
    C.prototype.__proto__ = X.prototype;

    assert(X.__proto__ === RegExp);
    assert(C.__proto__ === X);
    assert(X.prototype.__proto__ === RegExp.prototype);
    assert(C.prototype.__proto__ === X.prototype);

    assert(accesses == "", "unexpected call to overridden props");
    let result = "searchme".search(obj);
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();

// 2 levels of RegExp subclasses with proxied prototype.
(function () {
    let accesses = [];

    class B extends RegExp { };

    assert(B.__proto__ === RegExp);
    assert(B.prototype.__proto__ === RegExp.prototype);

    let proxy = new Proxy(RegExp.prototype, {
        get: function(obj, prop) {
            accesses.push("get_" + prop.toString());

            function proxyExec(str) {
                accesses.push("exec");
                return /rch/.exec(str);
            }

            if (prop === "exec")
                return proxyExec;
            return obj[prop];
        },
        set: function(obj, prop, value) {
            accesses.push("set_" + prop.toString());
        }
    });
    B.prototype.__proto__ = proxy;

    let obj = new B();

    assert(accesses == "", "unexpected call to overridden props");
    let result = "searchme".search(obj);
    assert(accesses == "get_Symbol(Symbol.search),get_exec,exec", "Property accesses do not match expectation");
    assert(result === 3, "Unexpected result");
})();

// Proxied RegExp observing every get.
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
    let result = "searchme".search(proxy);
    assert(accesses.toString() == "Symbol(Symbol.search),lastIndex,exec,lastIndex", "Proxy not able to observe some gets");
    assert(result === 3, "Unexpected result");
})();
