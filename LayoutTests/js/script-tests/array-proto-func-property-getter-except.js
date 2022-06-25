description(
"This test checks that functions on the array prototype correctly handle exceptions from property getters when called on non-array objects."
);

function test(f) {

    var testObj = {
        length: 3
    };
    var propertyGetter = {
        get: (function() { throw true; })
    }
    Object.defineProperty(testObj, 0, propertyGetter);
    Object.defineProperty(testObj, 1, propertyGetter);
    Object.defineProperty(testObj, 2, propertyGetter);

    try {
        f.call(testObj, function(){});
        return false;
    } catch (e) {
        return e === true;
    }
}

// This test makes sense for these functions: (they should get all properties on the array)
shouldBeTrue("test(Array.prototype.sort)");
shouldBeTrue("test(Array.prototype.every)");
shouldBeTrue("test(Array.prototype.some)");
shouldBeTrue("test(Array.prototype.forEach)");
shouldBeTrue("test(Array.prototype.map)");
shouldBeTrue("test(Array.prototype.filter)");
shouldBeTrue("test(Array.prototype.reduce)");
shouldBeTrue("test(Array.prototype.reduceRight)");

// Probably not testing much of anything in these cases, but make sure they don't crash!
shouldBeTrue("test(Array.prototype.join)");
shouldBeTrue("test(Array.prototype.pop)");
shouldBeFalse("test(Array.prototype.push)");
shouldBeTrue("test(Array.prototype.reverse)");
shouldBeTrue("test(Array.prototype.shift)");
shouldBeTrue("test(Array.prototype.slice)");
shouldBeTrue("test(Array.prototype.splice)");
shouldBeTrue("test(Array.prototype.unshift)");
shouldBeTrue("test(Array.prototype.indexOf)");
shouldBeTrue("test(Array.prototype.lastIndexOf)");
