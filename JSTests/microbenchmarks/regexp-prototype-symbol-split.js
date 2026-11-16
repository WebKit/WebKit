// Tracks the steady-state cost of `RegExp.prototype[Symbol.split]` after the C++ migration.
// Tuned to finish under ~100ms wall-clock on a release build.

var input = `Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed
do eiusmod tempor incididunt ut labore et dolore magna aliqua.
Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.`;

var whitespace = /\s+/;
var commaOrSemi = /[,;]\s*/;

function symbolSplit(string, regexp)
{
    return regexp[Symbol.split](string);
}
noInline(symbolSplit);

for (var i = 0; i < 3e4; ++i)
    symbolSplit(input, whitespace);

for (var i = 0; i < 8e3; ++i)
    symbolSplit(input, commaOrSemi);
