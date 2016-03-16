function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function unreachable()
{
    throw new Error('unreachable');
}

function assert(b) {
    if (!b)
        throw new Error("bad assertion");
}

{
    let target = {
        x: 30
    };

    let called = false;
    let handler = {
        set: 45
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        let threw = false;
        try {
            Reflect.set(proxy, 'x', 40);
            unreachable();
        } catch(e) {
            assert(e.toString() === "TypeError: 'set' property of a Proxy's handler should be callable.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {
        x: 30
    };

    let error = null;
    let handler = {
        get set() {
            error = new Error;
            throw error;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        let threw = false;
        try {
            Reflect.set(proxy, 'x', 40);
            unreachable();
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
        error = null;
    }
}

{
    let target = {
        x: 30
    };

    let error = null;
    let handler = {
        set: function() {
            error = new Error;
            throw error;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        let threw = false;
        try {
            Reflect.set(proxy, 'x', 40);
            unreachable();
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
        error = null;
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", {
        configurable: false,
        writable: false,
        value: 500
    });

    let called = false;
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(theTarget === target);
            called = true;
            theTarget[propName] = value;
            return false;    
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(proxy, 'x', 40), false);
        assert(called);
        assert(proxy.x === 500);
        assert(target.x === 500);
        called = false;
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", {
        configurable: false,
        writable: false,
        value: 500
    });

    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(theTarget === target);
            theTarget[propName] = value;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        let threw = false;
        try {
            Reflect.set(proxy, 'x', 40);
            unreachable();
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy handler's 'set' on a non-configurable and non-writable property on 'target' should either return false or be the same value already on the 'target'.");
        }
        assert(threw);
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", {
        configurable: false,
        get: function() {
            return 25;
        }
    });

    let called = false;
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(theTarget === target);
            called = true;
            theTarget[propName] = value;
            return false;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(proxy, 'x', 40), false);
        assert(proxy.x === 25);
        assert(called);
        called = false;
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", {
        configurable: false,
        get: function() {
            return 25;
        }
    });

    let called = false;
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(theTarget === target);
            called = true;
            theTarget[propName] = value;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        let threw = false;
        try {
            Reflect.set(proxy, 'x', 40);
            unreachable();
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy handler's 'set' method on a non-configurable accessor property without a setter should return false.");
        }
        assert(threw);
    }
}

{
    let target = { };
    Object.defineProperty(target, "x", {
        configurable: false,
        writable: true,
        value: 50
    });

    let called = false;
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(theTarget === target);
            called = true;
            theTarget[propName] = value;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(proxy, 'x', i), true);
        assert(called);
        assert(proxy.x === i);
        assert(target.x === i);
        called = false;
    }
}

{
    let target = {
        x: 30
    };

    let called = false;
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(target === theTarget);
            assert(receiver === proxy);
            called = true;
            theTarget[propName] = value;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(proxy, 'x', i), false);
        assert(called);
        assert(proxy.x === i);
        assert(target.x === i);
        called = false;

        shouldBe(Reflect.set(proxy, 'y', i), false);
        assert(called);
        assert(proxy.y === i);
        assert(target.y === i);
        called = false;
    }
}

{
    let target = {
        x: 30
    };

    let called = false;
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(target === theTarget);
            assert(receiver === proxy);
            called = true;
            theTarget[propName] = value;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(proxy, 'x', i), false);
        assert(called);
        assert(proxy.x === i);
        assert(target.x === i);
        called = false;

        shouldBe(Reflect.set(proxy, 'y', i), false);
        assert(called);
        assert(proxy.y === i);
        assert(target.y === i);
        called = false;
    }
}

{
    let target = [];

    let called = false;
    let handler = { };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(proxy, i, i), true);
        assert(proxy[i] === i);
        assert(target[i] === i);
    }
}

{
    let target = [];

    let called = false;
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(target === theTarget);
            assert(receiver === proxy);
            called = true;
            theTarget[propName] = value;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(proxy, i, i), false);
        assert(proxy[i] === i);
        assert(target[i] === i);
        assert(called);
        called = false;
    }
}

{
    let target = [];

    let called = false;
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(target === theTarget);
            assert(receiver === proxy);
            called = true;
            theTarget[propName] = value;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(proxy, i, i), false);
        assert(proxy[i] === i);
        assert(target[i] === i);
        assert(called);
        called = false;
    }
}

{
    let called = false;
    let target = {
        set x(v) {
            assert(this === target);
            this._x = v;
            called = true;
        },
        get x() {
            assert(this === target);
            return this._x;
        }
    };

    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(target === theTarget);
            assert(receiver === proxy);
            theTarget[propName] = value;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(proxy, 'x', i), true);
        assert(called);
        assert(proxy.x === i);
        assert(target.x === i);
        assert(proxy._x === i);
        assert(target._x === i);
        called = false;
    }
}

{
    let called = false;
    let target = {};
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(target === theTarget);
            assert(receiver === obj);
            theTarget[propName] = value;
            called = true;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    let obj = Object.create(proxy, {
        own: {
            writable: true,
            configurable: true,
            value: null
        }
    });
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(obj, 'own', i), true);
        assert(!called);
        assert(obj.own === i);

        shouldBe(Reflect.set(obj, 'notOwn', i), true);
        assert(target.notOwn === i);
        assert(proxy.notOwn === i);
        assert(obj.notOwn === i);
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let handler = { };

    let proxy = new Proxy(target, handler);
    let obj = Object.create(proxy, {
        own: {
            writable: true,
            configurable: true,
            value: null
        }
    });
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(obj, 'own', i), true);
        assert(obj.own === i);
        assert(proxy.own === undefined);

        shouldBe(Reflect.set(obj, 'notOwn', i), true);
        // The receiver is always |obj|.
        // obj.[[Set]](P, V, obj) -> Proxy.[[Set]](P, V, obj) -> target.[[Set]](P, V, obj)
        assert(target.notOwn === undefined);
        assert(proxy.notOwn === undefined);
        assert(obj.notOwn === i);
    }
}

{
    let called = false;
    let target = {};
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(target === theTarget);
            assert(receiver === obj);
            theTarget[propName] = value;
            called = true;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    let obj = Object.create(proxy, {
        [0]: {
            writable: true,
            configurable: true,
            value: null
        }
    });
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(obj, 0, i), true);
        assert(!called);
        assert(obj[0] === i);
        assert(proxy[0] === undefined);

        shouldBe(Reflect.set(obj, 1, i), true);
        assert(target[1] === i);
        assert(proxy[1] === i);
        assert(obj[1] === i);
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let handler = { };

    let proxy = new Proxy(target, handler);
    let obj = Object.create(proxy, {
        [0]: {
            writable: true,
            configurable: true,
            value: null
        }
    });
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(obj, 0, i), true);
        assert(obj[0] === i);
        assert(proxy[0] === undefined);

        shouldBe(Reflect.set(obj, 1, i), true);
        // The receiver is always |obj|.
        // obj.[[Set]](P, V, obj) -> Proxy.[[Set]](P, V, obj) -> target.[[Set]](P, V, obj)
        assert(target[1] === undefined);
        assert(proxy[1] === undefined);
        assert(obj[1] === i);
    }
}

{
    let called = false;
    let target = {};
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(target === theTarget);
            assert(receiver === obj);
            theTarget[propName] = value;
            called = true;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    let obj = Object.create(proxy, {
        [0]: {
            writable: true,
            configurable: true,
            value: null
        }
    });
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(obj, 0, i), true);
        assert(!called);
        assert(obj[0] === i);
        assert(proxy[0] === undefined);

        shouldBe(Reflect.set(obj, 1, i), true);
        assert(target[1] === i);
        assert(proxy[1] === i);
        assert(obj[1] === i);
        assert(called);
        called = false;
    }
}

{
    let called = false;
    let target = [25];
    let handler = {
        set: function(theTarget, propName, value, receiver) {
            assert(target === theTarget);
            assert(receiver === obj);
            theTarget[propName] = value;
            called = true;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    let obj = Object.create(proxy, {
        [0]: {
            writable: true,
            configurable: true,
            value: null
        }
    });
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(obj, 0, i), true);
        assert(!called);
        assert(obj[0] === i);
        assert(proxy[0] === 25);

        shouldBe(Reflect.set(obj, 1, i), true);
        assert(target[1] === i);
        assert(proxy[1] === i);
        assert(obj[1] === i);
        assert(called);
        called = false;
    }
}

{
    let called = false;
    let ogTarget = {};
    let target = new Proxy(ogTarget, {
        set: function(theTarget, propName, value, receiver) {
            assert(theTarget === ogTarget);
            assert(receiver === obj);
            called = true;
            theTarget[propName] = value;
        }
    });
    let handler = { };

    let proxy = new Proxy(target, handler);
    let obj = Object.create(proxy, {
        own: {
            writable: true,
            configurable: true,
            value: null
        }
    });
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(obj, 'own', i), true);
        assert(!called);
        assert(obj.own === i);
        assert(proxy.own === undefined);

        shouldBe(Reflect.set(obj, 'notOwn', i), false);
        assert(target.notOwn === i);
        assert(proxy.notOwn === i);
        assert(obj.notOwn === i);
        assert(called);
        called = false;
    }
}

{
    let called = false;
    let ogTarget = [25];
    let target = new Proxy(ogTarget, {
        set: function(theTarget, propName, value, receiver) {
            assert(theTarget === ogTarget);
            assert(receiver === obj);
            called = true;
            theTarget[propName] = value;
        }
    });
    let handler = { };

    let proxy = new Proxy(target, handler);
    let obj = Object.create(proxy, {
        [0]: {
            writable: true,
            configurable: true,
            value: null
        }
    });
    for (let i = 0; i < 1000; i++) {
        shouldBe(Reflect.set(obj, 0, i), true);
        assert(!called);
        assert(obj[0] === i);
        assert(proxy[0] === 25);

        shouldBe(Reflect.set(obj, 1, i), false);
        assert(target[1] === i);
        assert(proxy[1] === i);
        assert(obj[1] === i);
        assert(called);
        called = false;
    }
}
