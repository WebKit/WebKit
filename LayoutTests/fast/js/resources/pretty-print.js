description(
"This test checks that regexps and strings with special characters are pretty-printed correctly"
);

function f() {
    var re = /test/g;
    var s = '\n\r\\';
}

eval(f.toString());

var successfullyParsed = true;
