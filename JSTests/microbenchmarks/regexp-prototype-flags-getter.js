// Test performance of RegExp.prototype.flags getter.
var regexps = [/[a-z]+\/\d+/gi, /^\s+|\s+$/g, /(\w+)-(\d+)/, /foo[\s\S]*?bar/m];
var result = 0;
for (var i = 0; i < 2e6; ++i)
    result += regexps[i & 3].flags.length;
if (result !== 2000000)
    throw new Error("Bad result: " + result);
