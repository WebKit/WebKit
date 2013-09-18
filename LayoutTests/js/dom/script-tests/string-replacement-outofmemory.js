description(
'This tests that string replacement with a large replacement string causes an out-of-memory exception. See <a href="https://bugs.webkit.org/show_bug.cgi?id=102956">bug 102956</a> for more details.'
);

function createStringWithRepeatedChar(c, multiplicity) {
    while (c.length < multiplicity)
        c += c;
    c = c.substring(0, multiplicity);
    return c;
}

var x = "1";
var y = "2";
x = createStringWithRepeatedChar(x, 1 << 12);
y = createStringWithRepeatedChar(y, (1 << 20) + 1);

shouldThrow("x.replace(/\\d/g, y)", '"Error: Out of memory"');
var successfullyParsed = true;
