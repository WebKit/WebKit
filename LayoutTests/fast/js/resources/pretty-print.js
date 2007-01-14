description(
"This test checks that regexps, strings with special characters, object literals with non-identifier names, and array literals are pretty-printed correctly"
);

function f() {
    var re = /test/g;
    var s = '\n\r\\';
}

function g() {
    var o = {"#000": 1};
    return ["a", "b", "c"];
}

eval(f.toString());

eval(g.toString());
shouldBe("g().toString()", "['a', 'b', 'c'].toString()");

var successfullyParsed = true;
