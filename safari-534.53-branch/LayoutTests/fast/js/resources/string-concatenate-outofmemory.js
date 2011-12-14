description(
'This test checks if repeated string concatenation causes an exception (and not a crash). From WebKit Bug <a href="http://bugs.webkit.org/show_bug.cgi?id=11131">Repeated string concatenation results in OOM crash</a>.'
);

shouldThrow('s = "a"; while (1) { s += s; }', '"Error: Out of memory"'); // Expand at end of string
shouldThrow('s = "a"; while (1) { s += ("a" + s); }', '"Error: Out of memory"'); // Expand at beginning of string
shouldThrow('s = "a"; while (1) { s = [s, s].join(); }', '"Error: Out of memory"'); // Expand using UString::append.

debug('');
debug(
'We also verify that the the string is stil functional after the out of memory exception is raised.  In <a href="rdar://problem/5352887">rdar://problem/5352887</a>, accessing the string after the exception would crash.'
);

function ensureStringIsUsable(testName, stringName, str) {
    str[str.length - 1];
    try { [str, str].join(str); } catch (o) { }
    try { "a" + str; } catch (o) { }
    try { str + "a"; } catch (o) { }
    debug('PASS: String ' + stringName + ' was functional after ' + testName + ' raised out of memory exception.');
}

var s = "a";
try {
    while (1)
        s += s; // This will expand the string at the end using UString::expandCapacity
} catch (o) {
    ensureStringIsUsable('expandCapacity', 's', s);
}

s = "a";
var t = "";
try {
    while (1) {
        t = "a" + s;
        s += t; // This will expand the string at the beginning using UString::expandPreCapacity
    }
} catch (o) {
    // Ensure both strings involved are unharmed
    ensureStringIsUsable('expandPreCapacity', 's', s);
    ensureStringIsUsable('expandPreCapacity', 't', t);
}
delete t;

s = "a";
try {
    while (1)
        s = [s, s].join(); // This will expand the string using UString::append.
} catch (o) {
    ensureStringIsUsable('append', 's', s);
}

var successfullyParsed = true;
