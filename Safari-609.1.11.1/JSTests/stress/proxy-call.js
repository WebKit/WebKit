function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

{
    let target = function foo(...args) {
        assert(args[0] === 10);
        assert(args[1] === 20);
        return "foo";
    }
    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            assert(theTarget === target);
            assert(argArray[0] === 10);
            assert(argArray[1] === 20);
            return theTarget.apply(thisArg, argArray);
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy(10, 20) === "foo");
    }
}

{
    let target = function foo() { }
    let error = null;
    let handler = {
        get apply() {
            error = new Error();
            throw error;
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            proxy(10, 20);
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
    }
}

{
    let called = false;
    let globalThis = this;
    let target = function foo() {
        assert(this === globalThis);
        called = true;
    }
    let handler = {
        apply: null
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        proxy();
        assert(called);
        called = false;
    }
}

{
    let called = false;
    let globalThis = this;
    let target = function foo() {
        assert(this === globalThis);
        called = true;
    }
    let handler = {
        apply: undefined
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        proxy();
        assert(called);
        called = false;
    }
}

{
    let called = false;
    let thisValue = {};
    let target = function foo(x, y, z) {
        assert(this === thisValue);
        assert(x === 20);
        assert(y === 45);
        assert(z === "foo");
        called = true;
    }

    let handler = {
        apply: undefined
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        proxy.call(thisValue, 20, 45, "foo");
        assert(called);
        called = false;
        proxy.apply(thisValue, [20, 45, "foo"]);
        assert(called);
        called = false;
    }
}

{
    let called = false;
    let thisValue = {};
    let target = function foo(x, y, z) {
        assert(this === thisValue);
        assert(x === 20);
        assert(y === 45);
        assert(z === "foo");
        called = true;
        return this;
    }

    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            assert(theTarget === target);
            assert(argArray[0] === 20);
            assert(argArray[1] === 45);
            assert(argArray[2] === "foo");
            assert(thisArg === thisValue);
            return theTarget.apply(thisArg, argArray);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy.call(thisValue, 20, 45, "foo") === thisValue);
        assert(called);
        called = false;
        assert(proxy.apply(thisValue, [20, 45, "foo"]) === thisValue);
        assert(called);
        called = false;
    }
}

{
    let called = false;
    let target = Error;

    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            called = true;
            assert(theTarget === Error);
            return theTarget.apply(thisArg, argArray);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let error = proxy();
        assert(!!error.stack);
        called = false;
    }
}

{
    let called = false;
    let self = this;
    let target = (x) => {
        called = true;
        assert(this === self);
        return x;
    };

    let handler = { };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = proxy(i);
        assert(result === i);
        called = false;
    }
}

{
    let called = false;
    let self = this;
    let target = (x) => {
        assert(this === self);
        return x;
    };

    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            called = true;
            assert(theTarget === target);
            return theTarget.apply(thisArg, argArray);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = proxy(i);
        assert(result === i);
        called = false;
    }
}

{
    let called = false;
    let self = this;
    let target = (x) => {
        assert(this === self);
        return x;
    };

    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            called = true;
            assert(theTarget === target);
            return theTarget.apply(null, argArray);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = proxy(i);
        assert(called);
        assert(result === i);
        called = false;
    }
}

{
    let called = false;
    let target = (x) => { };
    let error = null;

    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            error = new Error();
            throw error;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            proxy();
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
    }
}

{
    let called = false;
    let error = null;
    let target = (x) => {
        error = new Error();
        throw error;
    };

    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            assert(theTarget === target);
            return theTarget.apply(null, argArray);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            proxy();
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
    }
}

{
    let called = false;
    let error = null;
    let target = new Proxy((x) => x, {});

    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            assert(theTarget === target);
            called = true;
            return theTarget.apply(null, argArray);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(proxy(i) === i);
        assert(called);
        called = false;
    }
}

{
    let target = (x) => x;
    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            return theTarget.apply(thisArg, argArray);
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(typeof proxy === "function");
    }
}

{
    let target = function() { }
    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            return theTarget.apply(thisArg, argArray);
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(typeof proxy === "function");
    }
}

{
    let target = Error;
    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            return theTarget.apply(thisArg, argArray);
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(typeof proxy === "function");
    }
}

{
    let target = (function foo() { }).bind({});
    let handler = {
        apply: function(theTarget, thisArg, argArray) {
            return theTarget.apply(thisArg, argArray);
        }
    };
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(typeof proxy === "function");
    }
}

{
    let target = function() { };
    let handler = {};
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(typeof proxy === "function");
    }
}

{
    let target = {};
    let handler = {};
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(typeof proxy === "object");
    }
}

{
    let target = [];
    let handler = {};
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(typeof proxy === "object");
    }
}

{
    let target = new String("foo");
    let handler = {};
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        assert(typeof proxy === "object");
    }
}
