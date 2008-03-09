description("This test covers the behaviour of pattern use and construction");

function dataToArray(data) {
    var result = new Array(data.length)
    for (var i = 0; i < data.length; i++)
        result[i] = data[i];
    return result;
}

function getPixel(x, y) {
    var data = context.getImageData(x,y,1,1);
    if (!data) // getImageData failed, which should never happen
        return [-1,-1,-1,-1];
    return dataToArray(data.data);
}

function pixelShouldBe(x, y, colour) {
    shouldBe("getPixel(" + [x, y] +")", "["+colour+"]");
}

var green1x1 = document.createElement("canvas");
green1x1.width = 1;
green1x1.height = 1;
var context = green1x1.getContext("2d");
context.fillStyle = "green";
context.fillRect(0,0,1,1);

var canvas = document.createElement("canvas");
canvas.width = 100;
canvas.height = 50;
var context = canvas.getContext("2d");
var tests = [
    function () {
        var didThrow = false;
        try {
            var pattern = context.createPattern(green1x1, null);
        } catch (e) {
            didThrow = true;
            testFailed("context.createPattern(green1x1, null) threw exception "+e)
        }
        if (!didThrow)
            testPassed("context.createPattern(green1x1, null) did not throw an exception");

        context.fillStyle = pattern;
        context.fillRect(0, 0, 100, 50);
        pixelShouldBe(1,   1, [0,128,0,255]);
        pixelShouldBe(98,  1, [0,128,0,255]);
        pixelShouldBe(1,  48, [0,128,0,255]);
        pixelShouldBe(98, 48, [0,128,0,255]);
    },
    function () {
        shouldThrow("context.createPattern(green1x1, 'null')");
    },
    function () {
        shouldThrow("context.createPattern(green1x1, undefined)");
    },
    function () {
        shouldThrow("context.createPattern(green1x1, 'undefined')");
    },
    function () {
        shouldThrow("context.createPattern(green1x1, {toString:function(){ return null;}})");
    },
    function () {
        var didThrow = false;
        try {
            var pattern = context.createPattern(green1x1, '');
        } catch (e) {
            didThrow = true;
            testFailed("context.createPattern(green1x1, '') threw exception "+e)
        }
        if (!didThrow)
            testPassed("context.createPattern(green1x1, '') did not throw an exception");

        context.fillStyle = pattern;
        context.fillRect(0, 0, 100, 50);
        pixelShouldBe(1,   1, [0,128,0,255]);
        pixelShouldBe(98,  1, [0,128,0,255]);
        pixelShouldBe(1,  48, [0,128,0,255]);
        pixelShouldBe(98, 48, [0,128,0,255]);
    },
    function () {
        var didThrow = false;
        try {
            var pattern = context.createPattern(green1x1, {toString:function(){ return 'repeat';}});
        } catch (e) {
            didThrow = true;
            testFailed("context.createPattern(green1x1, {toString:function(){ return 'repeat';}}) threw exception "+e)
        }
        if (!didThrow)
            testPassed("context.createPattern(green1x1, {toString:function(){ return 'repeat';}}) did not throw an exception");

        context.fillStyle = pattern;
        context.fillRect(0, 0, 100, 50);
        pixelShouldBe(1,   1, [0,128,0,255]);
        pixelShouldBe(98,  1, [0,128,0,255]);
        pixelShouldBe(1,  48, [0,128,0,255]);
        pixelShouldBe(98, 48, [0,128,0,255]);
    },
];
for (var i = 0; i < tests.length; i++) {
    context.fillStyle="red";
    context.fillRect(0,0,100,50);
    tests[i]();
}
var successfullyParsed = true;
