description("This tests that setting window event listeners on the body, sets them on the window.");

var func = function() { }

document.body.onblur = func;
shouldBe("window.onblur", "func");
shouldBe("window.onblur", "document.body.onblur");

document.body.onfocus = func;
shouldBe("window.onfocus", "func");
shouldBe("window.onfocus", "document.body.onfocus");

document.body.onerror = func;
shouldBe("window.onerror", "func");
shouldBe("window.onerror", "document.body.onerror");

document.body.onload = func;
shouldBe("window.onload", "func");
shouldBe("window.onload", "document.body.onload");

document.body.onbeforeunload = func;
shouldBe("window.onbeforeunload", "func");
shouldBe("window.onbeforeunload", "document.body.onbeforeunload");

document.body.onhashchange = func;
shouldBe("window.onhashchange", "func");
shouldBe("window.onhashchange", "document.body.onhashchange");

document.body.onmessage = func;
shouldBe("window.onmessage", "func");
shouldBe("window.onmessage", "document.body.onmessage");

document.body.onoffline = func;
shouldBe("window.onoffline", "func");
shouldBe("window.onoffline", "document.body.onoffline");

document.body.ononline = func;
shouldBe("window.ononline", "func");
shouldBe("window.ononline", "document.body.ononline");

document.body.onresize = func;
shouldBe("window.onresize", "func");
shouldBe("window.onresize", "document.body.onresize");

document.body.onstorage = func;
shouldBe("window.onstorage", "func");
shouldBe("window.onstorage", "document.body.onstorage");

document.body.onunload = func;
shouldBe("window.onunload", "func");
shouldBe("window.onunload", "document.body.onunload");
window.onunload = null;

var successfullyParsed = true;
