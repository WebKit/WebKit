function assert(b) {
    if (!b)
        throw new Error;
}

assert(JSON.stringify(JSON.parse('[1337,42]', function (x, y) {
    if (this instanceof Array) {
        Object.defineProperty(this, '1', {value: Array.prototype});
        return y;
    }
    return this;
})) === '{"":[1337,[]]}');

assert(JSON.stringify(JSON.parse('[0, 1]', function(x, y) {
    this[1] = Array.prototype;
    return y;
})) === '[0,[]]');

assert(JSON.stringify(JSON.parse('{"x":22, "y":44}', function(a, b) {
    this.y = Array.prototype;
    return b;
})) === '{"x":22,"y":[]}');

Array.prototype[0] = 42;
assert(JSON.stringify(JSON.parse('{"x":22, "y":44}', function(a, b) {
    this.y = Array.prototype;
    return b;
})) === '{"x":22,"y":[42]}');
