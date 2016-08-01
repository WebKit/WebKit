
var propertyKey = {
    toString() {
        throw new Error("propertyKey.toString is called.");
    }
};

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

var object = {};

shouldThrow(function () {
    object.hasOwnProperty(propertyKey);
}, "Error: propertyKey.toString is called.");

shouldThrow(function () {
    Object.prototype.hasOwnProperty.call(null, propertyKey);
}, "Error: propertyKey.toString is called.");

shouldThrow(function () {
    Object.prototype.hasOwnProperty.call(null, 'ok');
}, "TypeError: null is not an object (evaluating 'Object.prototype.hasOwnProperty.call(null, 'ok')')");

shouldThrow(function () {
    object.propertyIsEnumerable(propertyKey);
}, "Error: propertyKey.toString is called.");

// ToPropertyKey is first, ToObject is following.
shouldThrow(function () {
    Object.prototype.propertyIsEnumerable.call(null, propertyKey);
}, "Error: propertyKey.toString is called.");

shouldThrow(function () {
    // ToPropertyKey is first, ToObject is following.
    Object.prototype.propertyIsEnumerable.call(null, 'ok');
}, "TypeError: null is not an object (evaluating 'Object.prototype.propertyIsEnumerable.call(null, 'ok')')");

shouldThrow(function () {
    object.__defineGetter__(propertyKey, function () {
        return 'NG';
    });
}, "Error: propertyKey.toString is called.");

if (Object.getOwnPropertyDescriptor(object, ''))
    throw new Error("bad descriptor");

shouldThrow(function () {
    object.__defineSetter__(propertyKey, function () {
        return 'NG';
    });
}, "Error: propertyKey.toString is called.");

if (Object.getOwnPropertyDescriptor(object, ''))
    throw new Error("bad descriptor");

shouldThrow(function () {
    object.__lookupGetter__(propertyKey);
}, "Error: propertyKey.toString is called.");

shouldThrow(function () {
    object.__lookupSetter__(propertyKey);
}, "Error: propertyKey.toString is called.");
