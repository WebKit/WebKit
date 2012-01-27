description(
"This tests that the DFG JIT's optimizations for byte arrays work as expected."
);

function doPut(array, index, value) {
    array[index] = value;
}

function doGet(array, index) {
    return array[index];
}

canvas = document.createElement("canvas");
context = canvas.getContext("2d");
imageData = context.createImageData(10,10);
data = imageData.data;

shouldBe("data.length", "400");

for (var i = 0; i < 1000; ++i) {
    doPut(data, i % 100, i - 100);
    var expectedValue;
    if (i < 100)
        expectedValue = 0;
    else if (i > 355)
        expectedValue = 255;
    else
        expectedValue = i - 100;
    shouldBe("doGet(data, " + (i % 100) + ")", "" + expectedValue);
}

var successfullyParsed = true;
