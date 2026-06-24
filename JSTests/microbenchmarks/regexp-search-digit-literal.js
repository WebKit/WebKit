// Hot-path microbenchmark for String.prototype.search with a RegExp literal
// allocated on every call. Stresses the primordial-RegExp fast path together
// with regex allocation.

function search(string)
{
    return string.search(/[0-9]/);
}
noInline(search);

var string = "abc1def2ghi3jkl4mno5";
for (var i = 0; i < 1e6; ++i)
    search(string);
