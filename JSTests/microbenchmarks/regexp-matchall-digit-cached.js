// Hot-path microbenchmark for String.prototype.matchAll with a cached primordial
// RegExp argument. Exercises the C++ host function's fast path
// (isSymbolMatchAllFastAndNonObservable + inline iterator construction) plus the
// downstream RegExpStringIterator next() path.

function matchAll(string, regexp)
{
    return [...string.matchAll(regexp)];
}
noInline(matchAll);

var string = "abc1def2ghi3jkl4mno5";
var regexp = /[0-9]/g;
for (var i = 0; i < 3e5; ++i)
    matchAll(string, regexp);
