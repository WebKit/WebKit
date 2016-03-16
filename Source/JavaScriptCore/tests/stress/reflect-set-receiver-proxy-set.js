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

(function () {
    {
        let target = {
            x: 30
        };

        let called = false;
        let handler = {
            set: 45
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            let threw = false;
            try {
                Reflect.set(proxy, 'x', 40, theReceiver);
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

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            let threw = false;
            try {
                Reflect.set(proxy, 'x', 40, theReceiver);
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
            set: function(_1, _2, _3, receiver) {
                shouldBe(receiver, theReceiver);
                error = new Error;
                throw error;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            let threw = false;
            try {
                Reflect.set(proxy, 'x', 40, theReceiver);
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

        let theReceiver = {};
        let called = false;
        let handler = {
            set: function(theTarget, propName, value, receiver) {
                assert(theTarget === target);
                shouldBe(receiver, theReceiver);
                called = true;
                theTarget[propName] = value;
                return false;
            }
        };

        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(proxy, 'x', 40, theReceiver), false);
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
                shouldBe(receiver, theReceiver);
                theTarget[propName] = value;
                return true;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            let threw = false;
            try {
                Reflect.set(proxy, 'x', 40, theReceiver);
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
                shouldBe(receiver, theReceiver);
                called = true;
                theTarget[propName] = value;
                return false;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(proxy, 'x', 40, theReceiver), false);
            assert(proxy.x === 25);
            assert(theReceiver.x === undefined);
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
                shouldBe(receiver, theReceiver);
                called = true;
                theTarget[propName] = value;
                return true;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            let threw = false;
            try {
                Reflect.set(proxy, 'x', 40, theReceiver);
                unreachable();
            } catch(e) {
                threw = true;
                assert(e.toString() === "TypeError: Proxy handler's 'set' method on a non-configurable accessor property without a setter should return false.");
            }
            assert(called);
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
                shouldBe(receiver, theReceiver);
                called = true;
                theTarget[propName] = value;
                return true;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(proxy, 'x', i, theReceiver), true);
            assert(called);
            assert(proxy.x === i);
            assert(target.x === i);
            assert(theReceiver.x === undefined);
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
                shouldBe(receiver, theReceiver);
                called = true;
                theTarget[propName] = value;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(proxy, 'x', i, theReceiver), false);
            assert(called);
            assert(proxy.x === i);
            assert(target.x === i);
            assert(theReceiver.x === undefined);
            called = false;

            shouldBe(Reflect.set(proxy, 'y', i, theReceiver), false);
            assert(called);
            assert(proxy.y === i);
            assert(target.y === i);
            assert(theReceiver.y === undefined);
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
                assert(receiver === theReceiver);
                called = true;
                theTarget[propName] = value;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(proxy, 'x', i, theReceiver), false);
            assert(called);
            assert(proxy.x === i);
            assert(target.x === i);
            assert(theReceiver.x === undefined);
            called = false;

            shouldBe(Reflect.set(proxy, 'y', i, theReceiver), false);
            assert(called);
            assert(proxy.y === i);
            assert(target.y === i);
            assert(theReceiver.y === undefined);
            called = false;
        }
    }

    {
        let target = [];

        let called = false;
        let handler = { };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(proxy, i, i, theReceiver), true);
            assert(proxy[i] === undefined);
            assert(target[i] === undefined);
            assert(theReceiver[i] === i);
        }
    }

    {
        let target = [];

        let called = false;
        let handler = {
            set: function(theTarget, propName, value, receiver) {
                assert(target === theTarget);
                assert(receiver === theReceiver);
                called = true;
                theTarget[propName] = value;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(proxy, i, i, theReceiver), false);
            assert(proxy[i] === i);
            assert(target[i] === i);
            assert(theReceiver[i] === undefined);
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
                assert(receiver === theReceiver);
                theTarget[propName] = value;
                return true;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(proxy, 'x', i, theReceiver), true);
            assert(called);
            assert(proxy.x === i);
            assert(target.x === i);
            assert(theReceiver.x === undefined);
            assert(proxy._x === i);
            assert(target._x === i);
            assert(theReceiver._x === undefined);
            called = false;
        }
    }

    {
        let called = false;
        let target = {};
        let handler = {
            set: function(theTarget, propName, value, receiver) {
                assert(target === theTarget);
                assert(receiver === theReceiver);
                theTarget[propName] = value;
                called = true;
                return true;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        let obj = Object.create(proxy, {
            own: {
                writable: true,
                configurable: true,
                value: null
            }
        });
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(obj, 'own', i, theReceiver), true);
            assert(!called);
            assert(obj.own === null);
            assert(theReceiver.own === i);

            shouldBe(Reflect.set(obj, 'notOwn', i, theReceiver), true);
            assert(target.notOwn === i);
            assert(proxy.notOwn === i);
            assert(obj.notOwn === i);
            assert(theReceiver.notOwn === undefined);
            assert(called);
            called = false;
        }
    }

    {
        let target = {};
        let handler = { };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        let obj = Object.create(proxy, {
            own: {
                writable: true,
                configurable: true,
                value: null
            }
        });
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(obj, 'own', i, theReceiver), true);
            assert(obj.own === null);
            assert(proxy.own === undefined);
            assert(theReceiver.own === i);

            shouldBe(Reflect.set(obj, 'notOwn', i, theReceiver), true);
            // The receiver is always |theReceiver|.
            // obj.[[Set]](P, V, theReceiver) -> Proxy.[[Set]](P, V, theReceiver) -> target.[[Set]](P, V, theReceiver)
            assert(target.notOwn === undefined);
            assert(proxy.notOwn === undefined);
            assert(obj.notOwn === undefined);
            assert(theReceiver.notOwn === i);
        }
    }

    {
        let called = false;
        let target = {};
        let handler = {
            set: function(theTarget, propName, value, receiver) {
                assert(target === theTarget);
                assert(receiver === theReceiver);
                theTarget[propName] = value;
                called = true;
                return true;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        let obj = Object.create(proxy, {
            [0]: {
                writable: true,
                configurable: true,
                value: null
            }
        });
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(obj, 0, i, theReceiver), true);
            assert(!called);
            assert(obj[0] === null);
            assert(proxy[0] === undefined);
            assert(theReceiver[0] === i);

            shouldBe(Reflect.set(obj, 1, i, theReceiver), true);
            assert(target[1] === i);
            assert(proxy[1] === i);
            assert(obj[1] === i);
            assert(theReceiver[1] === undefined);
            assert(called);
            called = false;
        }
    }

    {
        let target = {};
        let handler = { };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        let obj = Object.create(proxy, {
            [0]: {
                writable: true,
                configurable: true,
                value: null
            }
        });
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(obj, 0, i, theReceiver), true);
            assert(obj[0] === null);
            assert(proxy[0] === undefined);
            assert(theReceiver[0] === i);

            shouldBe(Reflect.set(obj, 1, i, theReceiver), true);
            // The receiver is always |theReceiver|.
            // obj.[[Set]](P, V, theReceiver) -> Proxy.[[Set]](P, V, theReceiver) -> target.[[Set]](P, V, theReceiver)
            assert(target[1] === undefined);
            assert(proxy[1] === undefined);
            assert(obj[1] === undefined);
            assert(theReceiver[1] === i);
        }
    }

    {
        let called = false;
        let target = {};
        let handler = {
            set: function(theTarget, propName, value, receiver) {
                assert(target === theTarget);
                assert(receiver === theReceiver);
                theTarget[propName] = value;
                called = true;
                return true;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        let obj = Object.create(proxy, {
            [0]: {
                writable: true,
                configurable: true,
                value: null
            }
        });
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(obj, 0, i, theReceiver), true);
            assert(!called);
            assert(obj[0] === null);
            assert(proxy[0] === undefined);
            assert(theReceiver[0] === i);

            shouldBe(Reflect.set(obj, 1, i, theReceiver), true);
            assert(target[1] === i);
            assert(proxy[1] === i);
            assert(obj[1] === i);
            assert(theReceiver[1] === undefined);
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
                assert(receiver === theReceiver);
                theTarget[propName] = value;
                called = true;
                return true;
            }
        };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        let obj = Object.create(proxy, {
            [0]: {
                writable: true,
                configurable: true,
                value: null
            }
        });
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(obj, 0, i, theReceiver), true);
            assert(!called);
            assert(obj[0] === null);
            assert(proxy[0] === 25);
            assert(theReceiver[0] === i);

            shouldBe(Reflect.set(obj, 1, i, theReceiver), true);
            assert(target[1] === i);
            assert(proxy[1] === i);
            assert(obj[1] === i);
            assert(theReceiver[1] === undefined);
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
                assert(receiver === theReceiver);
                called = true;
                theTarget[propName] = value;
            }
        });
        let handler = { };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        let obj = Object.create(proxy, {
            own: {
                writable: true,
                configurable: true,
                value: null
            }
        });
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(obj, 'own', i, theReceiver), true);
            assert(!called);
            assert(obj.own === null);
            assert(proxy.own === undefined);
            assert(theReceiver.own === i);

            shouldBe(Reflect.set(obj, 'notOwn', i, theReceiver), false);
            assert(target.notOwn === i);
            assert(proxy.notOwn === i);
            assert(obj.notOwn === i);
            assert(theReceiver.notOwn === undefined);
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
                assert(receiver === theReceiver);
                called = true;
                theTarget[propName] = value;
            }
        });
        let handler = { };

        let theReceiver = {};
        let proxy = new Proxy(target, handler);
        let obj = Object.create(proxy, {
            [0]: {
                writable: true,
                configurable: true,
                value: null
            }
        });
        for (let i = 0; i < 1000; i++) {
            shouldBe(Reflect.set(obj, 0, i, theReceiver), true);
            assert(!called);
            assert(obj[0] === null);
            assert(proxy[0] === 25);
            assert(theReceiver[0] === i);

            shouldBe(Reflect.set(obj, 1, i, theReceiver), false);
            assert(target[1] === i);
            assert(proxy[1] === i);
            assert(obj[1] === i);
            assert(theReceiver[1] === undefined);
            assert(called);
            called = false;
        }
    }
}());
