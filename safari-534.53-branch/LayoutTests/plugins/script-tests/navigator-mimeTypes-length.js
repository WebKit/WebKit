description(
"Test for bug 10038: REGRESSION: Length of navigator.mimeTypes collection returns number of installed plugins, not number of registered mime types."
);

var numberOfMimeTypes = 0;
for (var i = 0; i < navigator.plugins.length; ++i) {
    var plugin = navigator.plugins[i];
    numberOfMimeTypes += plugin.length;
}

shouldBeTrue("navigator.mimeTypes.length == numberOfMimeTypes");
shouldBe("navigator.mimeTypes[navigator.mimeTypes.length+1]", "undefined");

successfullyParsed = true;
