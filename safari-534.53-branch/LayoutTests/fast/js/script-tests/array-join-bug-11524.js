description(
"This test verifies that Array.prototype.join uses the ToString operator rather than calling element.toString().  \
See <a href='http://bugs.webkit.org/show_bug.cgi?id=11524'>bug 11524</a> for more information."
);

var customObject = {toString: 0, valueOf: function() { return "custom object"; } };
shouldBe("[customObject].join()", "'custom object'");

successfullyParsed = true;
