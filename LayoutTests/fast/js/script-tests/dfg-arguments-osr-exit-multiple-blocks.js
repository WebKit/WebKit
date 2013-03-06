description(
"Tests aliased uses of 'arguments' that require reification of the Arguments object on OSR exit, in the case that there is some interesting control flow."
);

function baz() {
    return [variable];
}

function foo() {
    var result = 0;
    var a = arguments;
    for (var i = 0; i < a.length; ++i) {
        result += a[i];
        result += baz()[0];
        
        // Just some dummy control flow.
        if (result < 0)
            throw "Error";
    }
    return result;
}

function bar(x) {
    return foo(x);
}

var variable = 32;
var expected = "74";

for (var i = 0; i < 200; ++i) {
    if (i == 150) {
        variable = "32";
        expected = "\"4232\"";
    }
    
    shouldBe("bar(42)", expected);
}
