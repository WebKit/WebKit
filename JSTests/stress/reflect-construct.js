function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

shouldBe(Reflect.construct.length, 2);

shouldThrow(() => {
    Reflect.construct("hello", 42);
}, `TypeError: Reflect.construct requires the first argument be a constructor`);

shouldThrow(() => {
    Reflect.construct(Array.prototype.forEach, []);
}, `TypeError: Reflect.construct requires the first argument be a constructor`);

shouldThrow(() => {
    Reflect.construct(function () { }, 42, null);
}, `TypeError: Reflect.construct requires the third argument be a constructor if present`);

shouldThrow(() => {
    Reflect.construct(function () { }, 42, {});
}, `TypeError: Reflect.construct requires the third argument be a constructor if present`);

shouldThrow(() => {
    Reflect.construct(function () { }, 42, Array.prototype.forEach);
}, `TypeError: Reflect.construct requires the third argument be a constructor if present`);

shouldThrow(() => {
    Reflect.construct(function () { }, 42, function () { });
}, `TypeError: Reflect.construct requires the second argument be an object`);

shouldThrow(() => {
    var array = {
        get length() {
            throw new Error("ok");
        },
        get 0() {
            throw new Error("ng");
        }
    };
    Reflect.construct(function () { }, array);
}, `Error: ok`);

shouldThrow(() => {
    var array = {
        get length() {
            return 1;
        },
        get 0() {
            throw new Error("ok");
        }
    };
    Reflect.construct(function () { }, array);
}, `Error: ok`);

var array = {
    get length() {
        return 0;
    },
    get 0() {
        throw new Error("ng");
    }
};
shouldBe(Reflect.construct(function () { this.length = arguments.length; }, array).length, 0);

var globalObject = this;
shouldBe(Reflect.construct(function Hello() {
    "use strict";
    shouldBe(arguments[0], 0);
    shouldBe(arguments[1], 1);
    shouldBe(arguments[2], 2);
    shouldBe(typeof this, "object");
    shouldBe(new.target, Hello);
    this.result = arguments.length;
}, [0,1,2]).result, 3)

shouldBe(Reflect.construct(function Hello() {
    shouldBe(arguments[0], 0);
    shouldBe(arguments[1], 1);
    shouldBe(arguments[2], 2);
    shouldBe(typeof this, "object");
    shouldBe(new.target, Hello);
    this.result = arguments.length;
}, [0,1,2]).result, 3)

var newTarget = function () { };
shouldBe(Reflect.construct(function () {
    "use strict";
    shouldBe(new.target, newTarget);
    this.result = arguments.length;
}, [], newTarget).result, 0)

shouldBe(Reflect.construct(function () {
    shouldBe(new.target, newTarget);
    this.result = arguments.length;
}, [], newTarget).result, 0)

{
    class A {
        constructor()
        {
            this.type = "A";
        }
    }

    class B extends A {
        constructor()
        {
            super();
            this.type = "B";
        }
    }

    shouldBe(Reflect.construct(A, []).type, "A");
    shouldBe(Reflect.construct(B, []).type, "B");

    shouldBe(Reflect.construct(B, [], B).__proto__, B.prototype);
    shouldBe(Reflect.construct(B, [], A).__proto__, A.prototype);
    shouldBe(Reflect.construct(B, [], A).type, "B");
    shouldBe(Reflect.construct(B, [], B).type, "B");

    shouldBe(Reflect.construct(A, [], A).__proto__, A.prototype);
    shouldBe(Reflect.construct(A, [], B).__proto__, B.prototype);
    shouldBe(Reflect.construct(A, [], A).type, "A");
    shouldBe(Reflect.construct(A, [], B).type, "A");
}

function nativeConstructorTest()
{
    class DerivedMap {
    }
    shouldBe(Reflect.construct(Map, [], DerivedMap).__proto__, DerivedMap.prototype);
    let map = Reflect.construct(Map, [], DerivedMap);
    map.__proto__ = Map.prototype;
    map.set(20, 30);
    shouldBe(map.get(20), 30);

    class FailedMap {
    }
    shouldBe(Reflect.construct(FailedMap, [], Map).__proto__, Map.prototype);
    shouldThrow(() => {
        let map = Reflect.construct(FailedMap, [], Map);
        map.set(20, 30);
    }, `TypeError: Map operation called on non-Map object`);

    shouldBe(Reflect.construct(Set, [], Map).__proto__, Map.prototype);
    shouldThrow(() => {
        let map = Reflect.construct(Set, [], Map);
        map.set(20, 30);
    }, `TypeError: Map operation called on non-Map object`);

    let set = Reflect.construct(Set, [], Map);
    Set.prototype.add.call(set, 20);
    shouldBe(Set.prototype.has.call(set, 20), true);
}
noInline(nativeConstructorTest);

for (var i = 0; i < 1e4; ++i)
    nativeConstructorTest();

(function () {
    function Hello() { }
    let result = {};
    let proxy = new Proxy(Hello, {
        construct(theTarget, argArray, newTarget) {
            shouldBe(newTarget, Map);
            shouldBe(theTarget, Hello);
            shouldBe(argArray.length, 2);
            shouldBe(argArray[0], 10);
            shouldBe(argArray[1], 20);
            return result;
        }
    });
    shouldBe(Reflect.construct(proxy, [10, 20], Map), result);
}());

(function () {
    var proxy = new Proxy(Map, {
        construct(theTarget, argArray, newTarget) {
        }
    });

    var result = {};
    function Hello() {
        shouldBe(new.target, proxy);
        shouldBe(new.target.prototype, Map.prototype);
        shouldBe(arguments.length, 2);
        shouldBe(arguments[0], 10);
        shouldBe(arguments[1], 20);
        return result;
    }
    shouldBe(Reflect.construct(Hello, [10, 20], proxy), result);
}());

(function () {
    function Hello() { }
    var result = {};
    var proxy1 = new Proxy(Hello, {
        construct(theTarget, argArray, newTarget) {
            shouldBe(newTarget, proxy2);
            shouldBe(theTarget, Hello);
            shouldBe(argArray.length, 2);
            shouldBe(argArray[0], 10);
            shouldBe(argArray[1], 20);
            return result;
        }
    });

    var proxy2 = new Proxy(Map, {
        construct(theTarget, argArray, newTarget) {
        }
    });

    shouldBe(Reflect.construct(proxy1, [10, 20], proxy2), result);
}());
