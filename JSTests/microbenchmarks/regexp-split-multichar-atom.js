// Hot-path microbenchmark for String.prototype.split with a multi-character
// literal-atom RegExp separator. Exercises the SpecificPattern::Atom fast
// path with a separator longer than one character.

function split(string)
{
    return string.split(/--/);
}
noInline(split);

var string = "alpha--bravo--charlie--delta--echo--foxtrot--golf";
for (var i = 0; i < 1e6; ++i)
    split(string);
