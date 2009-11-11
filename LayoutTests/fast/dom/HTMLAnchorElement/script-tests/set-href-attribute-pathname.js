description('Test setting the pathname attribute of the URL in HTMLAnchorElement.');

var a = document.createElement('a');

debug("Set pathname that starts with slash");
a.href = "https://www.mydomain.com/path/testurl.html?key=value";
a.pathname = "/path name";
shouldBe("a.href", "'https://www.mydomain.com/path%20name?key=value'");

// IE8 throws an "Invalid URL" exception.
try {
debug("Set pathname that does not start with slash and contains '?'");
a.href = "https://www.mydomain.com/path/testurl.html?key=value";
a.pathname = "pa?th";
shouldBe("a.href", "'https://www.mydomain.com/pa%3Fth?key=value'");
} catch(e) {
debug("Exception: " + e.description);
}

// IE8 throws an "Invalid URL" exception.
try {
debug("Set pathname that starts with double slash and contains '#'");
a.href = "https://www.mydomain.com/path?key=value";
a.pathname = "//path#name";
shouldBe("a.href", "'https://www.mydomain.com//path%23name?key=value'");
} catch(e) {
debug("Exception: " + e.description);
}

debug("Set a pathname containing .. in it");
a.href = "https://www.mydomain.com/path/testurl.html?key=value";
a.pathname = "/it/../path";
shouldBe("a.href", "'https://www.mydomain.com/path?key=value'");

// IE8 converts null to "null", which is not the right thing to do.
debug("Set pathname to null");
a.href = "https://www.mydomain.com/path/testurl.html?key=value";
a.pathname = null;
shouldBe("a.href", "'https://www.mydomain.com/?key=value'");

debug("Set pathname to empty string");
a.href = "https://www.mydomain.com/?key=value";
a.pathname = "";
shouldBe("a.href", "'https://www.mydomain.com/?key=value'");

// The expected behavior should change when the character table is updated.
// IE8 considers this URL as valid.
debug("Set pathname that includes illegal characters to URL that contains illegal characters.");
a.href = "https://www.my|d[]()omain.com/path/testurl.html?key=value";
a.pathname = "p$a|th";
shouldBe("a.href", "'https://www.my|d[]()omain.com/path/testurl.html?key=value'");

// IE8 throws a security exception.
try {
debug("Set pathname to URL that contains '@' in host");
a.href = "http://w@#ww";
a.pathname = "path";
shouldBe("a.href", "'http://w@/path#ww'");
} catch(e) {
debug("Exception: " + e.description);
}

// IE8 allows setting the pathname, for non-hierarchial URL.
// It is not supposed to allow that per
// http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes .
debug("Set pathname to a URL with non-hierarchical protocol");
a.href = "tel:+1800-555-1212";
a.pathname = "the-path";
shouldBe("a.href", "'tel:+1800-555-1212'");

var successfullyParsed = true;
