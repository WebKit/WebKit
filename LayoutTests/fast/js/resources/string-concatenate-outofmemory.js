description(
'This test checks if repeated string concatenation causes an exception (and not a crash). From WebKit Bug <a href="http://bugs.webkit.org/show_bug.cgi?id=11131">Repeated string concatenation results in OOM crash</a>.'
);

shouldThrow('s = "a"; while (1) { s += s; }', '"Error: Out of memory"');

var successfullyParsed = true;
