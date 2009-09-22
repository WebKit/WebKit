description(
"instanceof XMLHttpRequest test"
);

shouldBeTrue("(new Audio()) instanceof Audio");
shouldBeFalse("(new Array()) instanceof Audio");

shouldBeTrue("(new Image()) instanceof Image");
shouldBeFalse("(new Array()) instanceof Image");

// MessageChannel is not available yet.
//shouldBeTrue("(new MessageChannel()) instanceof MessageChannel");
//shouldBeFalse("(new Array()) instanceof MessageChannel");

shouldBeTrue("(new Option()) instanceof Option");
shouldBeFalse("(new Array()) instanceof Option");

shouldBeTrue("(new WebKitCSSMatrix()) instanceof WebKitCSSMatrix");
shouldBeFalse("(new Array()) instanceof WebKitCSSMatrix");

shouldBeTrue("(new WebKitPoint()) instanceof WebKitPoint");
shouldBeFalse("(new Array()) instanceof WebKitPoint");

shouldBeTrue("(new Worker('instanceof-operator-dummy-worker.js')) instanceof Worker");
shouldBeFalse("(new Array()) instanceof Worker");

shouldBeTrue("(new XMLHttpRequest()) instanceof XMLHttpRequest")
shouldBeFalse("(new Array()) instanceof XMLHttpRequest")

shouldBeTrue("(new XSLTProcessor()) instanceof XSLTProcessor");
shouldBeFalse("(new Array()) instanceof XSLTProcessor");

var successfullyParsed = true;
