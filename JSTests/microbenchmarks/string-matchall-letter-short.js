// Hot-path microbenchmark for String.prototype.matchAll with a single-char literal
// pattern. Targets the stringMatchAllSlow path with a trivial regexp body.

function matchAll(string, regexp)
{
    return [...string.matchAll(regexp)];
}
noInline(matchAll);

var string = "alpha bravo charlie delta echo foxtrot golf";
for (var i = 0; i < 4e5; ++i)
    matchAll(string, "a");
