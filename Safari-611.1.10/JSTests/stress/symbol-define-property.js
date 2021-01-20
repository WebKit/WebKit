// This tests Object.create, Object.defineProperty, Object.defineProperties work with Symbol.

function testSymbol(object) {
    if (!object.hasOwnProperty(Symbol.iterator))
        throw "Error: object doesn't have Symbol.iterator";
    if (object.propertyIsEnumerable(Symbol.iterator))
        throw "Error: Symbol.iterator is defined as enumerable";
    if (JSON.stringify(Object.getOwnPropertyDescriptor(object, Symbol.iterator)) !== '{"value":42,"writable":false,"enumerable":false,"configurable":false}')
        throw "Error: bad property descriptor " + JSON.stringify(Object.getOwnPropertyDescriptor(object, Symbol.iterator));
    if (Object.getOwnPropertySymbols(object).length !== 1)
    throw "Error: bad value " + Object.getOwnPropertySymbols(object).length;
    if (Object.getOwnPropertySymbols(object)[0] !== Symbol.iterator)
        throw "Error: bad value " + String(Object.getOwnPropertySymbols(object)[0]);
}

var object = Object.create(Object.prototype, {
    [Symbol.iterator]: {
        value: 42
    }
});
testSymbol(object);

var object = Object.defineProperties({}, {
    [Symbol.iterator]: {
        value: 42
    }
});
testSymbol(object);

var object = Object.defineProperty({}, Symbol.iterator, {
    value: 42
});
testSymbol(object);
