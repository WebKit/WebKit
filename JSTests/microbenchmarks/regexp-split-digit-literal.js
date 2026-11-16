// Hot-path microbenchmark for String.prototype.split with a RegExp literal
// allocated on every call. Stresses the primordial-RegExp fast path together
// with regex allocation.

function split(string)
{
    return string.split(/[0-9]/);
}
noInline(split);

var string = "abc1def2ghi3jkl4mno5";
for (var i = 0; i < 1e6; ++i)
    split(string);
