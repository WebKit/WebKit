importScripts("../../resources/js-test.js");
// webgl-test-utils.js uses window, and reads properties of the document when it is loaded.
self.window = self;
self.document = {};
importScripts("webgl_test_files/js/webgl-test-utils.js");
delete self.document;
importScripts("../../http/wpt/webcodecs/resources/canvas-videoframe-copyto-utilities.js");
importScripts("videoframe-copyto-color-space-and-alpha.js");

testWebGLCanvasVideoFrames(["worker OffscreenCanvas"]).then(finishJSTest, error => {
    testFailed(`${error.name}: ${error.message}`);
    finishJSTest();
});
