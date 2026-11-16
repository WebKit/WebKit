var symbolSplit = RegExp.prototype[Symbol.split];

function split(regexp, string)
{
    return symbolSplit.call(regexp, string);
}
noInline(split);

var string = "abc";
var regexp = /x/;
for (var i = 0; i < 2e6; ++i)
    split(regexp, string);
