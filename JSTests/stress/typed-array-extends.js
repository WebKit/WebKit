// Crash on Debug builds
var array = [];
for (var i = 0; i < 1e4; ++i)
    array.push(i);
Reflect.defineProperty(array, Symbol.iterator, {
    get() {
        array.length = 0;
        Reflect.deleteProperty(array, Symbol.iterator);
        return undefined;
    },
    configurable: true
});
new Uint8Array(array);
