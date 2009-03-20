description("Test of various canvas graphics context calls for setting colors.");

var canvas = document.createElement("canvas");
var context = canvas.getContext('2d');

function clear()
{
    context.clearRect(0, 0, canvas.width, canvas.height);
}

function hex(number)
{
    var hexDigits = "0123456789abcdef";
    return hexDigits[number >> 4] + hexDigits[number & 0xF];
}

function pixel()
{
    var imageData = context.getImageData(0, 0, 1, 1);
    if (imageData.data[3] == 255)
        return "#" + hex(imageData.data[0]) + hex(imageData.data[1]) + hex(imageData.data[2]);
    if (imageData.data[3] == 0)
        return "rgba(" + imageData.data[0] + ", " + imageData.data[1] + ", " + imageData.data[2] + ", 0.0)";
    return "rgba(" + imageData.data[0] + ", " + imageData.data[1] + ", " + imageData.data[2] + ", " + (imageData.data[3] / 255) + ")";
}

function testFillStyle(string)
{
    clear();
    context.fillStyle = "black";
    context.fillStyle = string;
    context.fillRect(0, 0, 1, 1);
    return pixel();
}

function testFillGradient(string)
{
    clear();
    context.fillStyle = "black";
    var gradient = context.createLinearGradient(0, 0, 1, 1);
    gradient.addColorStop(0, string);
    gradient.addColorStop(1, string);
    context.fillStyle = gradient;
    context.fillRect(0, 0, 1, 1);
    return pixel();
}

function testSetFillColor(arguments)
{
    clear();
    context.fillStyle = "black";
    eval("context.setFillColor(" + arguments + ")");
    context.fillRect(0, 0, 1, 1);
    return pixel();
}

function testStrokeStyle(string)
{
    clear();
    context.lineWidth = 5;
    context.strokeStyle = "black";
    context.strokeStyle = string;
    context.strokeRect(2, 2, 10, 10);
    return pixel();
}

function testStrokeGradient(string)
{
    clear();
    context.lineWidth = 5;
    context.strokeStyle = "black";
    var gradient = context.createLinearGradient(0, 0, 1, 1);
    gradient.addColorStop(0, string);
    gradient.addColorStop(1, string);
    context.strokeStyle = gradient;
    context.strokeRect(2, 2, 10, 10);
    return pixel();
}

function testSetStrokeColor(arguments)
{
    clear();
    context.lineWidth = 5;
    context.strokeStyle = "black";
    eval("context.setStrokeColor(" + arguments + ")");
    context.strokeRect(2, 2, 10, 10);
    return pixel();
}

var transparent = "rgba(0, 0, 0, 0.0)";
var red = "#ff0000";
var green = "#00ff00";
var blue = "#0000ff";
var white = "#ffffff";
var translucentRed = "rgba(255, 0, 0, 0.8)";
var translucentGreen = "rgba(0, 255, 0, 0.8)";
var translucentBlue = "rgba(0, 0, 255, 0.8)";
var translucentWhite = "rgba(255, 255, 255, 0.8)";

shouldBe("testFillStyle('transparent')", "transparent");
shouldBe("testFillStyle('blue')", "blue");
shouldBe("testFillStyle('#FF0000')", "red");
shouldBe("testFillStyle('#f00')", "red");
shouldBe("testFillStyle('rgb(255, 0, 0)')", "red");
shouldBe("testFillStyle('rgba(255, 0, 0, 1)')", "red");
shouldBe("testFillStyle('rgba(255, 0, 0, 0.8)')", "translucentRed");
shouldBe("testFillStyle('rgba(255, 0, 0, 0)')", "transparent");
shouldBe("testFillGradient('transparent')", "transparent");
shouldBe("testFillGradient('blue')", "blue");
shouldBe("testFillGradient('#FF0000')", "red");
shouldBe("testFillGradient('#f00')", "red");
shouldBe("testFillGradient('rgb(255, 0, 0)')", "red");
shouldBe("testFillGradient('rgba(255, 0, 0, 1)')", "red");
shouldBe("testFillGradient('rgba(255, 0, 0, 0.8)')", "translucentRed");
shouldBe("testFillGradient('rgba(255, 0, 0, 0)')", "transparent");
shouldBe("testSetFillColor('\"blue\"')", "blue");
shouldBe("testSetFillColor('\"#FF0000\"')", "red");
shouldBe("testSetFillColor('\"#f00\"')", "red");
shouldBe("testSetFillColor('\"rgb(255, 0, 0)\"')", "red");
shouldBe("testSetFillColor('\"rgba(255, 0, 0, 1)\"')", "red");
shouldBe("testSetFillColor('\"rgba(255, 0, 0, 0.8)\"')", "translucentRed");
shouldBe("testSetFillColor('\"rgba(255, 0, 0, 0)\"')", "transparent");
shouldBe("testSetFillColor('\"blue\", 0.8')", "translucentBlue");
shouldBe("testSetFillColor('1')", "white");
shouldBe("testSetFillColor('1, 0.8')", "translucentWhite");
shouldBe("testSetFillColor('0, 1, 0, 1')", "green");
shouldBe("testSetFillColor('0, 1, 0, 0.8')", "translucentGreen");
shouldBe("testSetFillColor('0, 0, 0, 1, 1')", "'#1a1a1a'");                   // This test is expected to fail on older versions of Mac OS X.
shouldBe("testSetFillColor('0, 0, 0, 1, 0.8')", "'rgba(25, 25, 25, 0.8)'");   // Ditto.
shouldBe("testSetFillColor('0, 0, 0, 1, 0')", "transparent");
shouldBe("testStrokeStyle('transparent')", "transparent");
shouldBe("testStrokeStyle('blue')", "blue");
shouldBe("testStrokeStyle('#FF0000')", "red");
shouldBe("testStrokeStyle('#f00')", "red");
shouldBe("testStrokeStyle('rgb(255, 0, 0)')", "red");
shouldBe("testStrokeStyle('rgba(255, 0, 0, 1)')", "red");
shouldBe("testStrokeStyle('rgba(255, 0, 0, 0.8)')", "translucentRed");
shouldBe("testStrokeStyle('rgba(255, 0, 0, 0)')", "transparent");
shouldBe("testStrokeGradient('transparent')", "transparent");
shouldBe("testStrokeGradient('blue')", "blue");
shouldBe("testStrokeGradient('#FF0000')", "red");
shouldBe("testStrokeGradient('#f00')", "red");
shouldBe("testStrokeGradient('rgb(255, 0, 0)')", "red");
shouldBe("testStrokeGradient('rgba(255, 0, 0, 1)')", "red");
shouldBe("testStrokeGradient('rgba(255, 0, 0, 0.8)')", "translucentRed");
shouldBe("testStrokeGradient('rgba(255, 0, 0, 0)')", "transparent");
shouldBe("testSetStrokeColor('\"blue\"')", "blue");
shouldBe("testSetStrokeColor('\"#FF0000\"')", "red");
shouldBe("testSetStrokeColor('\"#f00\"')", "red");
shouldBe("testSetStrokeColor('\"rgb(255, 0, 0)\"')", "red");
shouldBe("testSetStrokeColor('\"rgba(255, 0, 0, 1)\"')", "red");
shouldBe("testSetStrokeColor('\"rgba(255, 0, 0, 0.8)\"')", "translucentRed");
shouldBe("testSetStrokeColor('\"rgba(255, 0, 0, 0)\"')", "transparent");
shouldBe("testSetStrokeColor('\"blue\", 0.8')", "translucentBlue");
shouldBe("testSetStrokeColor('1')", "white");
shouldBe("testSetStrokeColor('1, 0.8')", "translucentWhite");
shouldBe("testSetStrokeColor('0, 1, 0, 1')", "green");
shouldBe("testSetStrokeColor('0, 1, 0, 0.8')", "translucentGreen");
shouldBe("testSetStrokeColor('0, 0, 0, 1, 1')", "'#1a1a1a'");                  // This test is expected to fail on older versions of Mac OS X.
shouldBe("testSetStrokeColor('0, 0, 0, 1, 0.8')", "'rgba(25, 25, 25, 0.8)'");  // Ditto.
shouldBe("testSetStrokeColor('0, 0, 0, 1, 0')", "transparent");

var successfullyParsed = true;
