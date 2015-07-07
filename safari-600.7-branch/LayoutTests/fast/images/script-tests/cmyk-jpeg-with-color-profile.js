description("Test that we can render a CMYK JPEG without color corruption.");

// This is an async test because it has to wait for WebKit to load an image.
jsTestIsAsync = true;

// The colors used for verifying the test results.
var red = 0, green = 0, blue = 0, alpha = 0;

// Create a canvas element. This element is used for pasting a CMYK JPEG and
// reading its pixels.
var canvas = document.createElement("canvas");
canvas.width = 64;
canvas.height = 64;

// Create an image object and load a CMYK JPEG.
var image = new Image();
image.onload = function() {
    // Paste the loaded JPEG ('resources/cmyk-jpeg.jpg') to the canvas.
    var context = canvas.getContext("2d");
    context.drawImage(image, 0, 0);

    // Read the pixels in the canvas and calculate their avarage values.
    // (DumpRenderTree always allows to read them.)
    var data = context.getImageData(0, 0, canvas.width, canvas.height);
    var pixels = canvas.width * canvas.height;
    for (var i = 0; i < pixels * 4; i += 4) {
        red += data.data[i + 0];
        green += data.data[i + 1];
        blue += data.data[i + 2];
        alpha += data.data[i + 3];
    }
    red   /= pixels;
    green /= pixels;
    blue  /= pixels;
    alpha /= pixels;

    // Even though the output colors depend on color-profiles (i.e. they depend
    // on devices), green must be the most prominent color because the source
    // image only consists of green. So, we test it.
    shouldBeTrue("green > red");
    shouldBeTrue("green > blue");

    // Notify this test has been finished.
    finishJSTest();
}
image.src = "resources/cmyk-jpeg.jpg";
