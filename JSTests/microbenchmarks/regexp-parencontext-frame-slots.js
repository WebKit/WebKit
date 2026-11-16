(function() {
    var result = 0;
    var n = 20000;

    var regExps = [
        /^(?:a+b+c+|d+e+f+)+!/,
        /^(?:(?:a+b+|c+d+)(?:e+f+|g+h+)(?:i+j+|k+l+)(?:m+n+|o+p+))+!/,
        /^(?:\w+=(?:\d+|"[^"]*"|'[^']*');\s*)+!/,
        /^(?:\/[a-z]+(?:-[a-z]+)*(?:\.[a-z]+(?:-[a-z]+)*)*)+!/,
    ];
    var inputs = [
        "aabbccddeeff".repeat(6),
        "aabbgghhiijjoopp".repeat(5),
        "key=12; name=\"va lue\"; x='y';".repeat(4),
        "/foo-bar.baz/qux.quux-corge".repeat(4),
    ];
    var matchInputs = inputs.map((input) => input + "!");
    var failInputs = inputs.map((input) => input + "?");

    for (var i = 0; i < n; ++i) {
        for (var j = 0; j < regExps.length; ++j) {
            if (regExps[j].test(matchInputs[j]))
                ++result;
            if (!regExps[j].test(failInputs[j]))
                ++result;
        }
    }

    if (result != n * regExps.length * 2)
        throw "Error: bad result: " + result;
})();
