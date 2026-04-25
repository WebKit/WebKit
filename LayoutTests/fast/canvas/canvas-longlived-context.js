description("This test ensures that Canvas and CanvasRenderingContext2D work correctly if the rendering context outlives the canvas element");

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

function prepareCanvas() {
    var context = document.createElement("canvas").getContext("2d");
    context.fillStyle = "green";
    context.fillRect(0,0,100,100);
    return context;
}

function clobberGC(count) {
    for (var i = 0; i < 5000; ++i)
        ({a: i*i*i*0.5+"str", b: i/Math.sqrt(i)});
    if (count > 0)
        clobberGC(count-1);
}

function test() {
    context = prepareCanvas();
    clobberGC(40);
    pixelShouldBe(50, 50, [0, 128, 0, 255]);
}
test();
