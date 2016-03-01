function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}

{
    let target = {};
    let error = null;
    let handler = {
        get preventExtensions() {
            error = new Error;
            throw error;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.preventExtensions(proxy);
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    let error = null;
    let handler = {
        preventExtensions: function() {
            error = new Error;
            throw error;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.preventExtensions(proxy);
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
    }
}

{
    let error = null;
    let target = new Proxy({}, {
        preventExtensions: function() {
            error = new Error;
            throw error;
        }
    });
    let handler = {
        preventExtensions: function(theTarget) {
            return Reflect.preventExtensions(theTarget);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.preventExtensions(proxy);
        } catch(e) {
            assert(e === error);
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    let handler = {
        preventExtensions: 45
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.preventExtensions(proxy);
        } catch(e) {
            assert(e.toString() === "TypeError: 'preventExtensions' property of a Proxy's handler should be callable.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    let handler = {
        preventExtensions: null
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.preventExtensions(proxy);
        assert(result);
        assert(!Reflect.isExtensible(target));
    }
}

{
    let target = {};
    let handler = {
        preventExtensions: undefined
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.preventExtensions(proxy);
        assert(result);
        assert(!Reflect.isExtensible(target));
    }
}

{
    let target = {};
    let handler = {
        preventExtensions: function(theTarget) {
            return true;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.preventExtensions(proxy);
        } catch(e) {
            assert(e.toString() === "TypeError: Proxy's 'preventExtensions' trap returned true even though its target is extensible. It should have returned false.");
            threw = true;
        }
        assert(threw);
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        preventExtensions: function(theTarget) {
            assert(theTarget === target);
            called = true;
            return Reflect.preventExtensions(theTarget);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.preventExtensions(proxy);
        assert(result);
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        preventExtensions: function(theTarget) {
            assert(theTarget === target);
            called = true;
            return false;
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.preventExtensions(proxy);
        assert(!result);
        assert(called);
        called = false;
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        preventExtensions: function(theTarget) {
            assert(theTarget === target);
            called = true;
            return Reflect.preventExtensions(theTarget);
        }
    };
    
    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.preventExtensions(proxy);
        assert(result);
        assert(called);
        called = false;

        // FIXME: This is true once we implement Proxy.[[IsExtensible]]
        // https://bugs.webkit.org/show_bug.cgi?id=154872
        /*
        assert(!Reflect.isExtensible(proxy));
        */

        assert(!Reflect.isExtensible(target));
        assert(!Object.isExtensible(target));
        assert(Object.isFrozen(target));
        assert(Object.isSealed(target));
    }
}

{
    for (let i = 0; i < 500; i++) {
        let target = {};
        let called = false;
        let handler = {
            preventExtensions: function(theTarget) {
                assert(theTarget === target);
                called = true;
                return Reflect.preventExtensions(theTarget);
            }
        };
        
        let proxy = new Proxy(target, handler);

        let result = Reflect.preventExtensions(proxy);
        assert(result);
        assert(called);
        called = false;

        // FIXME: This is true once we implement Proxy.[[IsExtensible]]
        // https://bugs.webkit.org/show_bug.cgi?id=154872
        /*
        assert(!Reflect.isExtensible(proxy));
        */

        assert(!Reflect.isExtensible(target));
        assert(!Object.isExtensible(target));
        assert(Object.isFrozen(target));
        assert(Object.isSealed(target));
    }
}

{
    for (let i = 0; i < 500; i++) {
        let target = {};
        let called = false;
        let handler = {
            preventExtensions: function(theTarget) {
                assert(theTarget === target);
                called = true;
                return Object.preventExtensions(theTarget);
            }
        };
        
        let proxy = new Proxy(target, handler);

        let result = Reflect.preventExtensions(proxy);
        assert(result);
        assert(called);
        called = false;

        // FIXME: This is true once we implement Proxy.[[IsExtensible]]
        // https://bugs.webkit.org/show_bug.cgi?id=154872
        /*
        assert(!Reflect.isExtensible(proxy));
        */

        assert(!Reflect.isExtensible(target));
        assert(!Object.isExtensible(target));
        assert(Object.isFrozen(target));
        assert(Object.isSealed(target));
    }
}
