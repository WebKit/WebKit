// Test performance of RegExp.prototype.global getter.
var re = /foo/gi;
var result = false;
for (var i = 0; i < 1e6; ++i)
    result = re.global;
if (result !== true)
    throw new Error("Bad result: " + result);
