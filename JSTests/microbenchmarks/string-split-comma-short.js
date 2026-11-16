// Hot-path microbenchmark for String.prototype.split with a short literal
// string separator. This exercises the DFG StringSplit fast path for the
// common case (CSV-style splitting).

function split(string, separator)
{
    return string.split(separator);
}
noInline(split);

var string = "alpha,bravo,charlie,delta,echo,foxtrot,golf";
for (var i = 0; i < 1e7; ++i)
    split(string, ",");
