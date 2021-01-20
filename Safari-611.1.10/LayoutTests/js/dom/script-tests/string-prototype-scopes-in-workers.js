importScripts('../../../resources/js-test-pre.js');

description("This test checks whether String.prototype methods correctly handle |this| if |this| becomes worker global scope.");

var global = this;
global.jsTestIsAsync = true;

var error = null;
try {
    var charAt = String.prototype.charAt;
    charAt();
} catch (e) {
    error = e;
}
shouldBe(`String(error)`, `"TypeError: Type error"`);
finishJSTest();
