// Hot-path microbenchmark for String.prototype.split with a literal-atom
// RegExp separator. Stresses the SpecificPattern::Atom fast path that
// dispatches to the cached string-separator split.

function split(string)
{
    return string.split(/,/);
}
noInline(split);

var string = "alpha,bravo,charlie,delta,echo,foxtrot,golf";
for (var i = 0; i < 1e6; ++i)
    split(string);
