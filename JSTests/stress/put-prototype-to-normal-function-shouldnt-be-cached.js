function opt(object, value) {
    object.prototype = value;
}

function main() {
    Function.prototype.__proto__ = new Proxy({}, {});

    const object = {
        nonConstructor() { }
    };

    function target() { }

    Function.prototype.__proto__ = Object.prototype;

    const nonConstructor = object.nonConstructor;

    Object.defineProperty(nonConstructor, 'prototype', {
        configurable: false,
        enumerable: false,
        writable: true,
        value: {}
    });

    target.prototype = {};
    Reflect.construct(Object, [], target);

    opt(nonConstructor, {});
    const newPrototype = {};
    opt(target, newPrototype);

    const result = Reflect.construct(Object, [], target);

    if (result.__proto__ !== newPrototype)
        throw new Error("Rare data of result was not cleared.");
}

main();