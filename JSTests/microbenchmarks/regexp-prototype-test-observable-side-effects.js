//@ runNoFTL

function assert(testedValue, msg) {
    if (!testedValue)
        throw Error(msg);
}

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
    let result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === true, "Unexpected result");

    accesses = [];
    obj = new SubRegExp(/not/);

    assert(accesses == "", "unexpected call to overridden props");
    result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === false, "Unexpected result");
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
    let result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === true, "Unexpected result");

    accesses = [];
    obj = new TestRegExp(/not/);

    assert(accesses == "", "unexpected call to overridden props");
    result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === false, "Unexpected result");
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
    let result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === true, "Unexpected result");

    accesses = [];
    obj = new RegExpC(/not/);

    assert(accesses == "", "unexpected call to overridden props");
    result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === false, "Unexpected result");
})();

// 2 levels of RegExp subclasses with substituted prototype before instantiation.
(function () {
    let accesses = [];
    let regExpForOverriddenExec = /rch/;

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
            return regExpForOverriddenExec.exec(str);
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
    let result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === true, "Unexpected result");

    accesses = [];
    regExpForOverriddenExec = /not/;

    assert(accesses == "", "unexpected call to overridden props");
    result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === false, "Unexpected result");
})();

// 2 levels of RegExp subclasses with substituted prototype after instantiation.
(function () {
    let accesses = [];
    let regExpForOverriddenExec = /rch/;

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
            return regExpForOverriddenExec.exec(str);
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
    let result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === true, "Unexpected result");

    accesses = [];
    regExpForOverriddenExec = /not/;

    assert(accesses == "", "unexpected call to overridden props");
    result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "exec", "Property accesses do not match expectation");
    assert(result === false, "Unexpected result");
})();

// 2 levels of RegExp subclasses with proxied prototype.
(function () {
    let accesses = [];
    let regExpForOverriddenExec = /rch/;

    class B extends RegExp { };

    assert(B.__proto__ === RegExp);
    assert(B.prototype.__proto__ === RegExp.prototype);

    let proxy = new Proxy(RegExp.prototype, {
        get: function(obj, prop) {
            accesses.push("get_" + prop.toString());

            function proxyExec(str) {
                accesses.push("exec");
                return regExpForOverriddenExec.exec(str);
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
    let result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "get_exec,exec", "Property accesses do not match expectation");
    assert(result === true, "Unexpected result");

    accesses = [];
    regExpForOverriddenExec = /not/;

    assert(accesses == "", "unexpected call to overridden props");
    result = RegExp.prototype.test.call(obj, "searchme");
    assert(accesses == "get_exec,exec", "Property accesses do not match expectation");
    assert(result === false, "Unexpected result");
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
    let result = RegExp.prototype.test.call(proxy, "searchme");
    assert(accesses.toString() == "exec", "Proxy not able to observe some gets");
    assert(result === true, "Unexpected result");

    accesses = [];
    regexp = new RegExp(/not/);
    proxy = new Proxy(regexp, {
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
    result = RegExp.prototype.test.call(proxy, "searchme");
    assert(accesses.toString() == "exec", "Proxy not able to observe some gets");
    assert(result === false, "Unexpected result");
})();
