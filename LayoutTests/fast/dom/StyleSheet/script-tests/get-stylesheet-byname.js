description("This test verifies that a StyleSheet object will be returned instead of a HTMLStyleElement when calling document.styleSheets named property getter.");

var styleElement = document.createElement("style");
styleElement.setAttribute("id", "test");
document.head.appendChild(styleElement);
shouldBe('document.styleSheets["test"]', 'styleElement.sheet');
