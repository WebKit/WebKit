description("Test of texImage2d and texSubImage2d");

var context = create3DContext();
var image = document.createElement("img");
var video = document.createElement("video");
var canvas2d = document.createElement("canvas");
var context2d = canvas2d.getContext("2d");
var imageData = context2d.createImageData(64, 64);
var array = new WebGLUnsignedByteArray([ 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255 ]);

shouldThrow("context.texImage2D(context.TEXTURE_2D)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, 64, 64, 0, context.RGBA, context.UNSIGNED_BYTE, null)");
shouldThrow("context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, 0, context.RGBA, context.UNSIGNED_BYTE, 0)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, 2, 2, 0, context.RGBA, context.UNSIGNED_BYTE, array)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, imageData, true)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, image)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, canvas2d, false)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, video, true, false)");

shouldThrow("context.texSubImage2D(context.TEXTURE_2D)");
shouldBeUndefined("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, 0, 0, context.RGBA, context.UNSIGNED_BYTE, null)");
// FIXME: The following test fails on JSC: https://bugs.webkit.org/show_bug.cgi?id=38024
shouldThrow("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, 0, 0, context.RGBA, context.UNSIGNED_BYTE, 0)");
shouldThrow("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, 0, context.UNSIGNED_BYTE, 0)");
shouldBeUndefined("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, imageData, false)");
shouldBeUndefined("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, image)");
shouldBeUndefined("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, canvas2d, true)");
shouldBeUndefined("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, video, false, true)");

successfullyParsed = true;
