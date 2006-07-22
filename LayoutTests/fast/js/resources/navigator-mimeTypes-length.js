description(
"Test for bug 10038: REGRESSION: Length of navigator.mimeTypes collection returns number of installed plugins, not number of registered mime types."
);

shouldBeTrue("navigator.mimeTypes.length > navigator.plugins.length");
shouldBe("navigator.mimeTypes[navigator.mimeTypes.length+1]", "undefined");

successfullyParsed = true;
