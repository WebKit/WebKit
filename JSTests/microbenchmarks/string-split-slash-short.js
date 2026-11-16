// Hot-path microbenchmark for String.prototype.split with a single-char
// literal separator. Targets the splitStringByOneCharacterImpl fast path.

function split(string, separator)
{
    return string.split(separator);
}
noInline(split);

var string = "a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p";
for (var i = 0; i < 1e7; ++i)
    split(string, "/");
