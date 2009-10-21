description("This test checks the names and descriptions of DOM exceptions matches what they should be.");

try {
    document.appendChild();
} catch (e) {
    // FIXME: Setting window.exception because shouldBe needs a global variable
    window.exception = e;
    shouldBeEqualToString("window.exception.name", "NOT_FOUND_ERR");
    debug(e.description);
}

try {
    document.appendChild(document.createElement());
} catch (e) {
    // FIXME: Setting window.exception because shouldBe needs a global variable
    window.exception = e;
    shouldBeEqualToString("window.exception.name", "HIERARCHY_REQUEST_ERR");
    debug(e.description);
}

try {
    document.createTextNode("foo").splitText(10000);
} catch (e) {
    // FIXME: Setting window.exception because shouldBe needs a global variable
    window.exception = e;
    shouldBeEqualToString("window.exception.name", "INDEX_SIZE_ERR");
    debug(e.description);
}

var successfullyParsed = true;
