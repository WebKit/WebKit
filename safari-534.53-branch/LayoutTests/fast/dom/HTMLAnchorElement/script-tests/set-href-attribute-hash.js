description('Test setting the hash attribute of the URL in HTMLAnchorElement.');

var a = document.createElement('a');

debug("Hash value does not start with '#'");
a.href = "https://www.mydomain.com:8080/path/testurl.html#middle";
a.hash = "hash-value";
shouldBe("a.href", "'https://www.mydomain.com:8080/path/testurl.html#hash-value'");

debug("Hash value starts with '#'");
a.href = "file:///path/testurl.html#middle";
a.hash = "#hash_value";
shouldBe("a.href", "'file:///path/testurl.html#hash_value'");

debug("'?' in hash value");
a.href = "http://www.mydomain.com/path/testurl.html#middle";
a.hash = "#hash?value";
shouldBe("a.href", "'http://www.mydomain.com/path/testurl.html#hash?value'");

// The expected behavior should change when character table is updated.
// IE8 and Firefox3.5.2 don't consider these characters as illegal.
debug("'#' in hash value, and illegal characters in hostname");
a.href = "https://www.my\"d(){}|~om?ain#com/path/testurl.html#middle";
a.hash = "#hash#value";
shouldBe("a.href", "'https://www.my\"d(){}|~om?ain#com/path/testurl.html#middle'");

// IE8 converts null to "null", which is not the right thing to do.
// Firefox 3.5.2 removes the '#' at the end, and it should per
// http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes .
debug("Set hash to null");
a.href = "https://www.mydomain.com/path/testurl.html#middle";
a.hash = null;
shouldBe("a.href", "'https://www.mydomain.com/path/testurl.html#'");

// Firefox 3.5.2 removes the '#' at the end, and it should per
// http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes .
debug("Set hash to empty string");
a.href = "https://www.mydomain.com/path/testurl.html#middle";
a.hash = "";
shouldBe("a.href", "'https://www.mydomain.com/path/testurl.html#'");

// Firefox 3.5.2 does not allow setting hash to mailto: scheme, and it should.
debug("Add hash to mailto: protocol");
a.href = "mailto:e-mail_address@goes_here";
a.hash = "hash-value";
shouldBe("a.href", "'mailto:e-mail_address@goes_here#hash-value'");

// IE8 does not percent-encode spaces in the hash component, but it does that
// in the path component.
debug("Add hash to file: protocol");
a.href = "file:///some path";
a.hash = "hash value";
shouldBe("a.href", "'file:///some%20path#hash value'");

debug("Set hash to '#'");
a.href = "http://mydomain.com#middle";
a.hash = "#";
shouldBe("a.href", "'http://mydomain.com/#'");

// Firefox 3.5.2 does not allow setting hash to foo: scheme, and it should.
debug("Add hash to non-standard protocol");
try {
a.href = "foo:bar";
a.hash = "#hash";
shouldBe("a.href", "'foo:bar#hash'");
} catch(e) {
debug("Exception: " + e.description);
}

var successfullyParsed = true;
