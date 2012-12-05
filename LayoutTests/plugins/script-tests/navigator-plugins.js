description(
    "Check that navigator.plugins does not contain empty plugin objects"
);

var hasInvalidPlugin = false;
for (var i = 0; i < navigator.plugins.length; ++i) {
    var plugin = navigator.plugins[i];
    if (!plugin.description || !plugin.name || !plugin.filename) {
        hasInvalidPlugin = true;
        break;
    }
}
shouldBeFalse("hasInvalidPlugin");

successfullyParsed = true;
