description("Test Object.getOwnPropertyDescriptor on window object.");

var __skip__ = {
    "__skip__" : 1,
    "opener" : 1, // Work around DumpRenderTree bug where previous tests add window properties
    "Components" : 1, // Work around Firefox exception

    // Don't log DumpRenderTree injected objects
    "layoutTestController" : 1, // Work around http://bugs.webkit.org/show_bug.cgi?id=11373
    "constructor" : 0,  // Workaround to include constructor in windowPropertyNames
    "GCController" : 1,
    "accessibilityController" : 1,
    "appleScriptController" : 1,
    "eventSender" : 1,
    "navigationController" : 1,
    "objCController" : 1,
    "objCPlugin" : 1,
    "objCPluginFunction" : 1,
    "plainText" : 1,
    "textInputController" : 1,
    // Ignore these properties because they do not exist in all implementations. They will be tested separately
    "WebGLRenderingContext" : 1,
    "WebGLActiveInfo" : 1,
    "WebGLBuffer" : 1,
    "WebGLFramebuffer" : 1,
    "WebGLProgram" : 1,
    "WebGLRenderbuffer" : 1,
    "WebGLShader" : 1,
    "WebGLTexture" : 1,
    "WebGLUniformLocation" : 1,
    "ArrayBuffer" : 1,
    "DataView" : 1,
    "Int8Array" : 1,
    "Uint8Array" : 1,
    "Int16Array" : 1,
    "Uint16Array" : 1,
    "Int32Array" : 1,
    "Uint32Array" : 1,
    "Float32Array" : 1,
    "Float64Array" : 1,
    "FileError" : 1,
    "FileReader" : 1,
    "WebKitBlobBuilder" : 1,
    "ondeviceorientation" : 1,
    // Ignore this property because it only appears in debug builds and not in release.
    "jscprint" : 1,
    // Ignore this property because it is not supported in all platforms.
    "webkitURL" : 1,
};

var windowPropertyNames = Object.getOwnPropertyNames(window)
    .filter(function(name) { return !__skip__[name]; })
    .sort();

for (var i = 0; i < windowPropertyNames.length; ++i)
    shouldBe("typeof Object.getOwnPropertyDescriptor(window, '" + windowPropertyNames[i] + "')", "'object'");

// Properties in the prototype should not return descriptors

var protoPropertySet = {};
for (var o = window.__proto__; o; o = o.__proto__) {
    var names = Object.getOwnPropertyNames(o);
    for (var i = 0; i < names.length; ++i)
        protoPropertySet[names[i]] = true;
}

var protoPropertyNames = [];
for (var name in protoPropertySet)
    protoPropertyNames.push(name);
protoPropertyNames.sort();

for (var i = 0; i < protoPropertyNames.length; ++i) {
    if (protoPropertyNames[i] == "constructor")
        continue;
    shouldBeUndefined("Object.getOwnPropertyDescriptor(window, '" + protoPropertyNames[i] + "')");
}

var successfullyParsed = true;
