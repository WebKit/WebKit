description(
"This tests that DFG generated code speculating SlowPutArrayStorageShape doesn't crash when seeing fast ArrayStorageShapes."
);

var slowPutArrayStorageArray = [ "slow" ];
var fastArrayStorageArray = [ "fast" ];
fastArrayStorageArray[1000] = 50;

var o = { a: 10 };
Object.defineProperties(o, {
    "0": {
        set: function(x) { this.a = x; },
    },
});    

slowPutArrayStorageArray.__proto__ = o;

function foo(a, isFast) {
    var result = 10;
    if (!a)
        return result;

    var doStuff = a[0] && isFast;
    if (doStuff)
        result = a[0] + 10;
    return result;
}

function test() {
    for (var k = 0; k < 5000; k++) {
        foo(slowPutArrayStorageArray, false);
        foo(fastArrayStorageArray, true);
    }
}

test();

var successfullyParsed = true;
