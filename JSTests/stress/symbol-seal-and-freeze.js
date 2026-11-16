// This tests Object.seal and Object.freeze affect on Symbol properties.

var object = {
    [Symbol.iterator]: 42
};

if (!object.hasOwnProperty(Symbol.iterator))
    throw "Error: object doesn't have Symbol.iterator";
if (JSON.stringify(Object.getOwnPropertyDescriptor(object, Symbol.iterator)) !== '{"value":42,"writable":true,"enumerable":true,"configurable":true}')
    throw "Error: bad property descriptor " + JSON.stringify(Object.getOwnPropertyDescriptor(object, Symbol.iterator));
if (Object.getOwnPropertySymbols(object).length !== 1)
    throw "Error: bad value " + Object.getOwnPropertySymbols(object).length;
if (Object.getOwnPropertySymbols(object)[0] !== Symbol.iterator)
    throw "Error: bad value " + String(Object.getOwnPropertySymbols(object)[0]);

Object.seal(object);
if (!object.hasOwnProperty(Symbol.iterator))
    throw "Error: object doesn't have Symbol.iterator";
if (JSON.stringify(Object.getOwnPropertyDescriptor(object, Symbol.iterator)) !== '{"value":42,"writable":true,"enumerable":true,"configurable":false}')
    throw "Error: bad property descriptor " + JSON.stringify(Object.getOwnPropertyDescriptor(object, Symbol.iterator));

Object.freeze(object);
if (!object.hasOwnProperty(Symbol.iterator))
    throw "Error: object doesn't have Symbol.iterator";
if (JSON.stringify(Object.getOwnPropertyDescriptor(object, Symbol.iterator)) !== '{"value":42,"writable":false,"enumerable":true,"configurable":false}')
    throw "Error: bad property descriptor " + JSON.stringify(Object.getOwnPropertyDescriptor(object, Symbol.iterator));
