description(
"This test checks that functions on the array prototype correctly handle exceptions from length getters when called on non-array objects."
);

var testObj = {
    0: "a",
    1: "b",
    2: "c"
};
var lengthGetter = {
    get: (function() { throw true; })
}
Object.defineProperty(testObj, "length", lengthGetter);

function test(f) {
    try {
        f.call(testObj, undefined);
        return false;
    } catch (e) {
        return e === true;
    }
}

shouldBeTrue("test(Array.prototype.join)");
shouldBeTrue("test(Array.prototype.pop)");
shouldBeTrue("test(Array.prototype.push)");
shouldBeTrue("test(Array.prototype.reverse)");
shouldBeTrue("test(Array.prototype.shift)");
shouldBeTrue("test(Array.prototype.slice)");
shouldBeTrue("test(Array.prototype.sort)");
shouldBeTrue("test(Array.prototype.splice)");
shouldBeTrue("test(Array.prototype.unshift)");
shouldBeTrue("test(Array.prototype.indexOf)");
shouldBeTrue("test(Array.prototype.lastIndexOf)");
shouldBeTrue("test(Array.prototype.every)");
shouldBeTrue("test(Array.prototype.some)");
shouldBeTrue("test(Array.prototype.forEach)");
shouldBeTrue("test(Array.prototype.map)");
shouldBeTrue("test(Array.prototype.filter)");
shouldBeTrue("test(Array.prototype.reduce)");
shouldBeTrue("test(Array.prototype.reduceRight)");

successfullyParsed = true;
