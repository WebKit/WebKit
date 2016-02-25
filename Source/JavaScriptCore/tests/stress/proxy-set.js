function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
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
            proxy.x = 40;
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
            proxy.x = 40;
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
            proxy.x = 40;
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
        set: function(theTarget, propName, value, reciever) {
            assert(theTarget === target);
            called = true;
            theTarget[propName] = value;
            return false;    
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        proxy.x = 40;
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
        set: function(theTarget, propName, value, reciever) {
            assert(theTarget === target);
            theTarget[propName] = value;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        let threw = false;
        try {
            proxy.x = 40;
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
        set: function(theTarget, propName, value, reciever) {
            assert(theTarget === target);
            called = true;
            theTarget[propName] = value;
            return false;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        proxy.x = 40;
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
        set: function(theTarget, propName, value, reciever) {
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
            proxy.x = 40;
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
        set: function(theTarget, propName, value, reciever) {
            assert(theTarget === target);
            called = true;
            theTarget[propName] = value;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        proxy.x = i;
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
        set: function(theTarget, propName, value, reciever) {
            assert(target === theTarget);
            assert(reciever === proxy);
            called = true;
            theTarget[propName] = value;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        proxy.x = i;
        assert(called);
        assert(proxy.x === i);
        assert(target.x === i);
        called = false;

        proxy["y"] = i;
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
        set: function(theTarget, propName, value, reciever) {
            assert(target === theTarget);
            assert(reciever === proxy);
            called = true;
            theTarget[propName] = value;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        proxy.x = i;
        assert(called);
        assert(proxy.x === i);
        assert(target.x === i);
        called = false;

        proxy["y"] = i;
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
        proxy[i] = i;
        assert(proxy[i] === i);
        assert(target[i] === i);
    }
}

{
    let target = [];

    let called = false;
    let handler = {
        set: function(theTarget, propName, value, reciever) {
            assert(target === theTarget);
            assert(reciever === proxy);
            called = true;
            theTarget[propName] = value;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        proxy[i] = i;
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
        set: function(theTarget, propName, value, reciever) {
            assert(target === theTarget);
            assert(reciever === proxy);
            called = true;
            theTarget[propName] = value;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        proxy[i] = i;
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
        set: function(theTarget, propName, value, reciever) {
            assert(target === theTarget);
            assert(reciever === proxy);
            theTarget[propName] = value;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 1000; i++) {
        proxy.x = i;
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
        set: function(theTarget, propName, value, reciever) {
            assert(target === theTarget);
            assert(reciever === obj);
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
        obj.own = i;
        assert(!called);
        assert(obj.own === i);

        obj.notOwn = i;
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
        obj.own = i;
        assert(obj.own === i);
        assert(proxy.own === undefined);

        obj.notOwn = i;
        assert(target.notOwn === i);
        assert(proxy.notOwn === i);
        assert(obj.notOwn === i);
    }
}

{
    let called = false;
    let target = {};
    let handler = {
        set: function(theTarget, propName, value, reciever) {
            assert(target === theTarget);
            assert(reciever === obj);
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
        obj[0] = i;
        assert(!called);
        assert(obj[0] === i);
        assert(proxy[0] === undefined);

        obj[1] = i;
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
        obj[0] = i;
        assert(obj[0] === i);
        assert(proxy[0] === undefined);

        obj[1] = i;
        assert(target[1] === i);
        assert(proxy[1] === i);
        assert(obj[1] === i);
    }
}

{
    let called = false;
    let target = {};
    let handler = {
        set: function(theTarget, propName, value, reciever) {
            assert(target === theTarget);
            //assert(reciever === obj);
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
        obj[0] = i;
        assert(!called);
        assert(obj[0] === i);
        assert(proxy[0] === undefined);

        obj[1] = i;
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
        set: function(theTarget, propName, value, reciever) {
            assert(target === theTarget);
            //assert(reciever === obj);
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
        obj[0] = i;
        assert(!called);
        assert(obj[0] === i);
        assert(proxy[0] === 25);

        obj[1] = i;
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
        set: function(theTarget, propName, value, reciever) {
            assert(theTarget === ogTarget);
            assert(reciever === obj);
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
        obj.own = i;
        assert(!called);
        assert(obj.own === i);
        assert(proxy.own === undefined);

        obj.notOwn = i;
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
        set: function(theTarget, propName, value, reciever) {
            assert(theTarget === ogTarget);
            assert(reciever === obj);
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
        obj[0] = i;
        assert(!called);
        assert(obj[0] === i);
        assert(proxy[0] === 25);

        obj[1] = i;
        assert(target[1] === i);
        assert(proxy[1] === i);
        assert(obj[1] === i);
        assert(called);
        called = false;
    }
}
