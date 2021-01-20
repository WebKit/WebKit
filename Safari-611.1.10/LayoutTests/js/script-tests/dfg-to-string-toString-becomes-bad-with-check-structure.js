description(
"Tests that the DFG checks that the toString method didn't become bad even if the StringObject already had a CheckStructure."
);

function foo() {
    return String(this);
}

for (var i = 0; i < 100; ++i) {
    if (i == 99)
        String.prototype.toString = function() { return 42; }
    shouldBe("foo.call(new String(\"foo\"))", i >= 99 ? "\"42\"" : "\"foo\"");
}

