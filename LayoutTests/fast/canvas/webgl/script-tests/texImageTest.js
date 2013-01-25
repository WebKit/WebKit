description("Test of texImage2d and texSubImage2d");

if (window.internals)
    window.internals.settings.setWebGLErrorsToConsoleEnabled(false);

var context = create3DContext();
var image = document.createElement("img");
var video = document.createElement("video");
var canvas2d = document.createElement("canvas");
var context2d = canvas2d.getContext("2d");
var imageData = context2d.createImageData(64, 64);
var array = new Uint8Array([ 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255 ]);

shouldThrow("context.texImage2D(context.TEXTURE_2D)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, 64, 64, 0, context.RGBA, context.UNSIGNED_BYTE, null)");
shouldThrow("context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, 0, context.RGBA, context.UNSIGNED_BYTE, 0)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, 2, 2, 0, context.RGBA, context.UNSIGNED_BYTE, array)");
shouldBeUndefined("context.pixelStorei(context.UNPACK_FLIP_Y_WEBGL, true)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, context.RGBA, context.UNSIGNED_BYTE, imageData)");
shouldBeUndefined("context.pixelStorei(context.UNPACK_FLIP_Y_WEBGL, false)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, context.RGBA, context.UNSIGNED_BYTE, image)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, context.RGBA, context.UNSIGNED_BYTE, canvas2d)");
shouldBeUndefined("context.pixelStorei(context.UNPACK_FLIP_Y_WEBGL, true)");
shouldBeUndefined("context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, context.RGBA, context.UNSIGNED_BYTE, video)");

shouldThrow("context.texSubImage2D(context.TEXTURE_2D)");
shouldBeUndefined("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, 0, 0, context.RGBA, context.UNSIGNED_BYTE, null)");
shouldThrow("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, 0, 0, context.RGBA, context.UNSIGNED_BYTE, 0)");
shouldThrow("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, 0, context.UNSIGNED_BYTE, 0)");
shouldBeUndefined("context.pixelStorei(context.UNPACK_FLIP_Y_WEBGL, false)");
shouldBeUndefined("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, context.RGBA, context.UNSIGNED_BYTE, imageData)");
shouldBeUndefined("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, context.RGBA, context.UNSIGNED_BYTE, image)");
shouldBeUndefined("context.pixelStorei(context.UNPACK_FLIP_Y_WEBGL, true)");
shouldBeUndefined("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, context.RGBA, context.UNSIGNED_BYTE, canvas2d)");
shouldBeUndefined("context.pixelStorei(context.UNPACK_FLIP_Y_WEBGL, false)");
shouldBeUndefined("context.pixelStorei(context.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true)");
shouldBeUndefined("context.texSubImage2D(context.TEXTURE_2D, 0, 10, 20, context.RGBA, context.UNSIGNED_BYTE, video)");
