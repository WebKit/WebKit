// Hot-path microbenchmark for String.prototype.search with a short literal
// string regexp source. This exercises the DFG StringSearch fast path for the
// common case where the regexp arg is a string and gets coerced via
// RegExp construction.

function search(string, regexp)
{
    return string.search(regexp);
}
noInline(search);

var string = "abc1def2ghi3jkl4mno5";
for (var i = 0; i < 1e7; ++i)
    search(string, "[0-9]");
