description("Regresion test for 155776. This test should pass and not crash.");

var bigArray = [];
var bigNum = 123456789;
var smallNum = 123;
var toStringCount = 0;

function fillBigArrayViaToString(n) {
    var results = [];

    for (var i = 0; i < n; i++)
        fillBigArrayViaToString.toString();

    return results;
}

Function.prototype.toString = function(x) {
    toStringCount++;
    bigArray.push(smallNum);

    if (toStringCount == 2000) {
        var newArray = new Uint32Array(8000);
        for (var i = 0; i < newArray.length; i++)
            newArray[i] = 0x10000000;
    }

    bigArray.push(fillBigArrayViaToString);
    bigArray.push(fillBigArrayViaToString);
    bigArray.push(fillBigArrayViaToString);
    return bigNum;
};

fillBigArrayViaToString(4000).join();

bigArray.length = 4000;

var stringResult = bigArray.join(":");

var expectedArray = [];

for (var i = 0; i < 1000; i++) {
    expectedArray.push(smallNum);
    expectedArray.push(bigNum);
    expectedArray.push(bigNum);
    expectedArray.push(bigNum);
}

var expectedString = expectedArray.join(":");

shouldBe('stringResult', 'expectedString');
