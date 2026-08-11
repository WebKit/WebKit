function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function defineCGetter(obj) {
    Reflect.defineProperty(obj, "c", {
        get: function() { return 'defineCGetter'; }
    });
}

class A {
    b = defineCGetter(this);
    c = 42;
};
shouldThrow(() => new A(), `TypeError: Attempting to change configurable attribute of unconfigurable property.`);

let key = 'c';
class B {
    b = defineCGetter(this);
    [key] = 42;
};
shouldThrow(() => new B(), `TypeError: Attempting to change configurable attribute of unconfigurable property.`);

function define0Getter(obj) {
    Reflect.defineProperty(obj, "0", {
        get: function() { return 'defineCGetter'; }
    });
}
class C {
    b = define0Getter(this);
    [0] = 42;
};
shouldThrow(() => new C(), `TypeError: Attempting to change configurable attribute of unconfigurable property.`);

class D {
    b = Object.freeze(this);
    [0] = 42;
};
shouldThrow(() => new D(), `TypeError: Attempting to define property on object that is not extensible.`);
