var symbolMatch = RegExp.prototype[Symbol.match];

function match(regexp, string)
{
    return symbolMatch.call(regexp, string);
}
noInline(match);

var string = "abc";
var regexp = /x/;
for (var i = 0; i < 1e7; ++i)
    match(regexp, string);
