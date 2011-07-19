description(
'This is a test case for <a https://bugs.webkit.org/show_bug.cgi?id=64678">bug 64678</a>.'
);

// These call pass undefined as this value, and as such should show in toObject.
shouldThrow("toString()");
shouldThrow("toLocaleString()");
shouldThrow("valueOf()");
shouldThrow("hasProperty('hasProperty')");
shouldThrow("propertyIsEnumerable('hasProperty')");
shouldThrow("isPrototypeOf(this)");

var successfullyParsed = true;
