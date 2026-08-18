//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function objectIsExtensiblePlain(o)
{
    return Object.isExtensible(o);
}
noInline(objectIsExtensiblePlain);

(function() {
    var o = {};
    for (var i = 0; i < 1e6; ++i) {
        if (objectIsExtensiblePlain(o) !== true)
            throw new Error("bad result");
    }
})();

function objectIsExtensibleNonExtensible(o)
{
    return Object.isExtensible(o);
}
noInline(objectIsExtensibleNonExtensible);

(function() {
    var o = Object.preventExtensions({});
    for (var i = 0; i < 1e6; ++i) {
        if (objectIsExtensibleNonExtensible(o) !== false)
            throw new Error("bad result");
    }
})();

function objectIsExtensibleSealedOrFrozen(o)
{
    return Object.isExtensible(o);
}
noInline(objectIsExtensibleSealedOrFrozen);

(function() {
    var sealed = Object.seal({ a: 1 });
    var frozen = Object.freeze({ b: 2 });
    for (var i = 0; i < 1e6; ++i) {
        var o = (i % 2 === 0) ? sealed : frozen;
        if (objectIsExtensibleSealedOrFrozen(o) !== false)
            throw new Error("bad result");
    }
})();

function objectIsExtensibleNotObject(v)
{
    return Object.isExtensible(v);
}
noInline(objectIsExtensibleNotObject);

(function() {
    for (var i = 0; i < 1e6; ++i) {
        if (objectIsExtensibleNotObject(42) !== false)
            throw new Error("bad result");
        if (objectIsExtensibleNotObject("str") !== false)
            throw new Error("bad result");
        if (objectIsExtensibleNotObject(null) !== false)
            throw new Error("bad result");
    }
})();

function objectIsExtensibleProxy(o)
{
    return Object.isExtensible(o);
}
noInline(objectIsExtensibleProxy);

(function() {
    var target = {};
    var proxy = new Proxy(target, {});
    for (var i = 0; i < 1e5; ++i) {
        if (objectIsExtensibleProxy(proxy) !== true)
            throw new Error("bad result");
    }
    Object.preventExtensions(target);
    for (var i = 0; i < 1e5; ++i) {
        if (objectIsExtensibleProxy(proxy) !== false)
            throw new Error("bad result");
    }
})();

function objectIsExtensibleMixed(v)
{
    return Object.isExtensible(v);
}
noInline(objectIsExtensibleMixed);

(function() {
    var extensible = {};
    var nonExtensible = Object.preventExtensions({});
    for (var i = 0; i < 1e6; ++i) {
        switch (i % 4) {
        case 0:
            objectIsExtensibleMixed(extensible);
            break;
        case 1:
            objectIsExtensibleMixed(nonExtensible);
            break;
        case 2:
            objectIsExtensibleMixed(7);
            break;
        case 3:
            objectIsExtensibleMixed("hi");
            break;
        }
    }
})();
