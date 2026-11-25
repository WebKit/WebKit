// RegExp.prototype.source getter for a long 16-bit pattern which has nothing to escape.
var pattern = "(?:[\u3042-\u3093]{1,4}:){6}|\\/x[/]".repeat(10);
var regExp = new RegExp(pattern);
var result = 0;
for (var i = 0; i < 1e6; ++i)
    result += regExp.source.length;
if (result !== pattern.length * 1e6)
    throw new Error("Bad result: " + result);
