description(
"Tests accessing arguments by val where the arguments need to be lazily created doesn't crash - original webkit bug# 120080"
);

function foo() {
    var r = arguments.length;
    return arguments[0];
}

for (var i = 0; i < 3000; ++i) {
    result = foo();
}

testPassed("No crash lazily creating arguments");

