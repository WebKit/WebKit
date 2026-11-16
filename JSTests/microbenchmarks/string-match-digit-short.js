// Hot-path microbenchmark for String.prototype.match with a short literal
// string regexp source. This exercises the DFG StringMatch fast path for the
// common case where the regexp arg is a string and gets coerced via
// RegExp construction.

function match(string, regexp)
{
    return string.match(regexp);
}
noInline(match);

var string = "abc1def2ghi3jkl4mno5";
for (var i = 0; i < 1e7; ++i)
    match(string, "[0-9]");
