// Hot-path microbenchmark for String.prototype.matchAll with a RegExp literal
// allocated on every call. Stresses the primordial-RegExp fast path together with
// regex allocation.

function matchAll(string)
{
    return [...string.matchAll(/[0-9]/g)];
}
noInline(matchAll);

var string = "abc1def2ghi3jkl4mno5";
for (var i = 0; i < 3e5; ++i)
    matchAll(string);
