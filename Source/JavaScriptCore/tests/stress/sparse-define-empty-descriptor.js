var array = [];
array[10000000] = 42;
Object.defineProperty(array, 10000000, {configurable: true, enumerable: true, writable: true});
var result = array[10000000];
if (result != 42)
    throw "Error: bad result: " + result;
