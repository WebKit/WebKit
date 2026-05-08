// Hot-path microbenchmark for String.prototype.split with a literal-atom
// RegExp separator splitting a path-like string. Mirrors the pattern seen
// in JetStream3 path-handling code.

function split(string)
{
    return string.split(/\//);
}
noInline(split);

var string = "a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p";
for (var i = 0; i < 1e6; ++i)
    split(string);
