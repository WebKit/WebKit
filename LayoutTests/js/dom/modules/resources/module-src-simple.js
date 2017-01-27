import Cocoa from "./module-src-simple-cocoa.js";
var cocoa = new Cocoa();

debug("Module execution is confined in the module environment.");
shouldBeEqualToString("typeof cocoa", "undefined");

window.exportedCocoa = cocoa;
shouldBeEqualToString("typeof exportedCocoa", "object");
shouldBeEqualToString("exportedCocoa.taste()", "nice");
finishJSTest();
