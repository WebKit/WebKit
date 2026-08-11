(function() {
    var result = 0;
    var n = 3000;

    function buildPattern(groupCount) {
        var parts = [];
        for (var i = 0; i < groupCount; ++i)
            parts.push("(x" + i + "|y" + i + ")+");
        return parts.join("") + "z";
    }

    function buildMatchingInput(groupCount) {
        var s = "";
        for (var i = 0; i < groupCount; ++i)
            s += (i % 2 === 0 ? "x" : "y") + i;
        return s + "z";
    }

    var groupCount = 250;
    var re = new RegExp(buildPattern(groupCount));
    var matchInput = buildMatchingInput(groupCount);
    // Missing the trailing "z" forces a backtrack back out through every group's ParenContext
    // (restoreParenContext) before the match finally fails.
    var failInput = matchInput.slice(0, -1);

    for (var i = 0; i < n; ++i) {
        if (re.exec(matchInput) !== null)
            ++result;
        if (re.exec(failInput) === null)
            ++result;
    }

    if (result != n * 2)
        throw "Error: bad result: " + result;
})();
