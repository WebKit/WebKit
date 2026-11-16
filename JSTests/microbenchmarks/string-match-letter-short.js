// Hot-path microbenchmark for String.prototype.match with a single-char
// literal pattern. Targets the simple non-global match fast path.

function match(string, regexp)
{
    return string.match(regexp);
}
noInline(match);

var string = "alpha bravo charlie delta echo foxtrot golf";
for (var i = 0; i < 1e7; ++i)
    match(string, "a");
