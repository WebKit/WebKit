// Hot-path microbenchmark for String.prototype.match with a RegExp literal
// allocated on every call. Stresses the primordial-RegExp fast path together
// with regex allocation.

function match(string)
{
    return string.match(/[0-9]/g);
}
noInline(match);

var string = "abc1def2ghi3jkl4mno5";
for (var i = 0; i < 1e6; ++i)
    match(string);
