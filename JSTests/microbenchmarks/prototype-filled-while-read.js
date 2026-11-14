function addMethods(target, from, to) {
    for (let i = from; i < to; ++i)
        target["method" + i] = function () { return i; };
}

// The object is read while it is still being filled. The read flattens the dictionary, and
// adding the rest of the methods turns it into a dictionary again before it becomes a prototype.
const prototype = {};
addMethods(prototype, 0, 150);
for (let i = 0; i < 150; ++i) {
    if (!prototype.hasOwnProperty("method" + i))
        throw new Error("missing method");
}
addMethods(prototype, 150, 300);

function Thing(value) {
    this.value = value;
}
Thing.prototype = prototype;

function test(thing) {
    let result = 0;
    for (let i = 0; i < 1e6; ++i) {
        result += thing.method3() + thing.method250();
        if (thing.hasOwnProperty("value"))
            result += thing.value;
        if (thing.absent !== undefined)
            result = 0;
    }
    return result;
}
noInline(test);

if (test(new Thing(1)) !== 254e6)
    throw new Error("bad result");
