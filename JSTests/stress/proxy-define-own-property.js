function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

{
    let target = {};
    let handler = {
        defineProperty: 25
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.defineProperty(proxy, "x", {
                enumerable: true,
                configurable: true,
                value: 55
            });
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: 'defineProperty' property of a Proxy's handler should be callable");
        }

        assert(threw);
    }

}

{
    let target = {};
    let error = null;
    let handler = {
        get defineProperty() {
            error = new Error;
            throw error;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.defineProperty(proxy, "x", {
                enumerable: true,
                configurable: true,
                value: 55
            });
        } catch(e) {
            threw = true;
            assert(e === error);
        }

        assert(threw);
    }
}

{
    let target = {};
    let error = null;
    let handler = {
        defineProperty: function() {
            error = new Error;
            throw error;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Object.defineProperty(proxy, "x", {
                enumerable: true,
                configurable: true,
                value: 55
            });
        } catch(e) {
            threw = true;
            assert(e === error);
        }

        assert(threw);
    }
}

{
    let target = {};
    let handler = {
        defineProperty: null
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        Object.defineProperty(proxy, "x", {
            enumerable: true,
            configurable: true,
            value: i
        });

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(pDesc.value === i);
            assert(pDesc.configurable === true);
            assert(pDesc.enumerable === true);
        }
    }
}

{
    let target = {};
    let handler = {
        defineProperty: undefined
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.defineProperty(proxy, "x", {
            enumerable: true,
            configurable: true,
            value: i
        });
        assert(result);

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(pDesc.value === i);
            assert(pDesc.configurable === true);
            assert(pDesc.enumerable === true);
        }
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            called = true;
            return Reflect.defineProperty(theTarget, propName, descriptor);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.defineProperty(proxy, "x", {
            enumerable: true,
            configurable: true,
            value: i
        });
        assert(result);
        assert(called);
        called = false;

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(pDesc.value === i);
            assert(pDesc.configurable === true);
            assert(pDesc.enumerable === true);
        }
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            called = true;
            Reflect.defineProperty(theTarget, propName, descriptor);
            return false;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.defineProperty(proxy, "x", {
            enumerable: true,
            configurable: true,
            value: i
        });
        assert(!result);
        assert(called);
        called = false;

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(pDesc.value === i);
            assert(pDesc.configurable === true);
            assert(pDesc.enumerable === true);
        }
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            called = true;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.defineProperty(proxy, "x", {
                enumerable: true,
                configurable: false,
                value: i
            });
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy's 'defineProperty' trap returned true for a non-configurable field even though getOwnPropertyDescriptor of the Proxy's target returned undefined");
        }
        assert(called);
        assert(threw);
        called = false;
    }
}

{
    let target = {};
    let called = false;
    Object.preventExtensions(target);
    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            called = true;
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.defineProperty(proxy, "x", {
                enumerable: true,
                configurable: true,
                value: i
            });
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy's 'defineProperty' trap returned true even though getOwnPropertyDescriptor of the Proxy's target returned undefined and the target is non-extensible");
        }
        assert(called);
        assert(threw);
        called = false;
    }
}

{
    let target = {};
    let called = false;
    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            assert(propName === "x");
            assert(descriptor.configurable === false);
            called = true;
            return Reflect.defineProperty(theTarget, "x", {
                enumerable: true,
                configurable: true,
                value: descriptor.value
            });
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        try {
            Reflect.defineProperty(proxy, "x", {
                enumerable: true,
                configurable: false,
                value: i
            });
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy's 'defineProperty' trap did not define a non-configurable property on its target even though the input descriptor to the trap said it must do so");
        }
        assert(called);
        assert(threw);
        called = false;

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(pDesc.value === i);
            assert(pDesc.configurable === true);
            assert(pDesc.enumerable === true);
        }
    }
}

{
    let target = {};
    let called = false;
    Object.defineProperty(target, "x", {
        configurable: false,
        writable: true,
        value: 55
    });
    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            assert(propName === "x");
            assert(descriptor.configurable === true);
            called = true;
            Reflect.defineProperty(theTarget, "x", {
                enumerable: true,
                configurable: true,
                value: descriptor.value
            });
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.defineProperty(proxy, "x", {
                configurable: true,
                writable: false,
                value: 45
            });
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy's 'defineProperty' trap did not define a property on its target that is compatible with the trap's input descriptor");
        }
        assert(called);
        assert(threw);
        called = false;

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(pDesc.value === 55);
            assert(pDesc.configurable === false);
        }
    }
}

{
    let target = {};
    let called = false;
    Object.defineProperty(target, "x", {
        configurable: false,
        writable: true,
        value: 55
    });
    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            assert(propName === "x");
            assert(descriptor.configurable === true);
            called = true;
            Reflect.defineProperty(theTarget, "x", descriptor);
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.defineProperty(proxy, "x", {
                configurable: true,
                set:function(){},
                get:function(){}
            });
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy's 'defineProperty' trap did not define a property on its target that is compatible with the trap's input descriptor");
        }
        assert(called);
        assert(threw);
        called = false;

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(pDesc.value === 55);
            assert(pDesc.configurable === false);
        }
    }
}

{
    let target = {};
    let called = false;
    let setter = function(){};
    let getter = function(){};
    Object.defineProperty(target, "x", {
        configurable: false,
        get: getter,
        set: setter
    });
    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            assert(propName === "x");
            assert(descriptor.configurable === true);
            called = true;
            Reflect.defineProperty(theTarget, "x", descriptor);
            return true;
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.defineProperty(proxy, "x", {
                configurable: true,
                value: 45
            });
        } catch(e) {
            threw = true;
            assert(e.toString() === "TypeError: Proxy's 'defineProperty' trap did not define a property on its target that is compatible with the trap's input descriptor");
        }
        assert(called);
        assert(threw);
        called = false;

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(pDesc.value === undefined);
            assert(pDesc.configurable === false);
            assert(pDesc.get === getter);
            assert(pDesc.set === setter);
        }
    }
}

{
    let target = {};
    let called = false;
    let setter = function(){};
    let getter = function(){};
    Object.defineProperty(target, "x", {
        configurable: false,
        get: getter,
        set: setter
    });
    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            assert(propName === "x");
            assert(descriptor.configurable === true);
            called = true;
            return Reflect.defineProperty(theTarget, "x", descriptor);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.defineProperty(proxy, "x", {
            configurable: true,
            value: 45
        });
        assert(!result);
        assert(called);
        called = false;

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(pDesc.value === undefined);
            assert(pDesc.configurable === false);
            assert(pDesc.get === getter);
            assert(pDesc.set === setter);
        }
    }
}

{
    let error = false;
    let target = new Proxy({}, {
        getOwnPropertyDescriptor: function() {
            error = new Error;
            throw error;
        }
    });

    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            return Reflect.defineProperty(theTarget, propName, descriptor);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            Reflect.defineProperty(proxy, "x", {
                configurable: true,
                value: 45
            });
        } catch(e) {
            threw = true;
            assert(e === error);
        }
        assert(threw);
    }
}

{
    let target = {};
    Reflect.defineProperty(target, "x", {
        writable: true,
        configurable: false,
        value: 55
    });

    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            assert(theTarget === target);
            return Reflect.defineProperty(theTarget, propName, descriptor);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.defineProperty(proxy, "x", {
            configurable: false,
            value: 55
        });
        assert(result);
        assert(target.x === 55);
        delete target.x;
        assert(target.x === 55);
    }
}

{
    let target = {};
    Reflect.defineProperty(target, "x", {
        writable: false,
        configurable: false,
        value: 55
    });

    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            assert(theTarget === target);
            return Reflect.defineProperty(theTarget, propName, descriptor);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.defineProperty(proxy, "x", {
            writable: false,
            configurable: false,
            value: 55
        });
        assert(result);
        assert(target.x === 55);
        delete target.x;
        assert(target.x === 55);
    }
}

{
    let target = {};
    Reflect.defineProperty(target, "x", {
        writable: false,
        configurable: false,
        value: 55
    });

    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            assert(theTarget === target);
            return Reflect.defineProperty(theTarget, propName, descriptor);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.defineProperty(proxy, "x", {
            writable: false,
            configurable: false,
            value: "not 55"
        });
        assert(!result);
        assert(target.x === 55);
        delete target.x;
        assert(target.x === 55);
    }
}

{
    let target = {};
    Reflect.defineProperty(target, "x", {
        writable: false,
        configurable: false,
        value: 55
    });

    let handler = {
        defineProperty: function(theTarget, propName, descriptor) {
            assert(theTarget === target);
            return Reflect.defineProperty(theTarget, propName, descriptor);
        }
    };

    let proxy = new Proxy(target, handler);
    for (let i = 0; i < 500; i++) {
        let result = Reflect.defineProperty(proxy, "x", {
            writable: true,
            configurable: false,
            value: "whatever value goes here"
        });
        assert(!result);
        assert(target.x === 55);
        delete target.x;
        assert(target.x === 55);
    }
}
