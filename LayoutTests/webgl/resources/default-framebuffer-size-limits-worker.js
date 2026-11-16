importScripts("../../resources/js-test.js");
// webgl-test-utils.js uses window, and reads properties of the document when it is loaded. The
// checks that the tests use do not use the document.
self.window = self;
self.document = {};
importScripts("webgl_test_files/js/webgl-test-utils.js");
delete self.document;
importScripts("default-framebuffer-size-limits.js");
// The page passes the part to run in the fragment, for example "#1-4" for the first of four parts.
var part = self.location.hash.substring(1).split("-").map(Number);
runOffscreenDefaultFramebufferSizeLimitTests(part[0], part[1]);
