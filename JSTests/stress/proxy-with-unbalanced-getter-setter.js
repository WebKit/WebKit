function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

// Setting the getter only.  
(function () {
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
            get: function(){},
        });
        assert(result);
        assert(called);
        called = false;

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(typeof pDesc.get === "function");
            assert(typeof pDesc.set === "undefined");
            assert(pDesc.get.toString() === (function(){}).toString());
            assert(pDesc.configurable === true);
            assert(pDesc.enumerable === true);
        }
    }
})();

// Setting the setter only.  
(function () {
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
            set: function(x){},
        });
        assert(result);
        assert(called);
        called = false;

        for (let obj of [target, proxy]) {
            let pDesc = Object.getOwnPropertyDescriptor(obj, "x");
            assert(typeof pDesc.get === "undefined");
            assert(typeof pDesc.set === "function");
            assert(pDesc.set.toString() === (function(x){}).toString());
            assert(pDesc.configurable === true);
            assert(pDesc.enumerable === true);
        }
    }
})();
