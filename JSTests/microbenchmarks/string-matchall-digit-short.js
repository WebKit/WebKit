// Hot-path microbenchmark for String.prototype.matchAll with a string regexp source.
// Exercises stringMatchAllSlow's RegExpCreate path (with the new
// (pattern, OptionSet<Yarr::Flags>) regExpCreate overload, which avoids allocating
// the "g" JSString) and its primordial-RegExp inline iterator construction.

function matchAll(string, regexp)
{
    return [...string.matchAll(regexp)];
}
noInline(matchAll);

var string = "abc1def2ghi3jkl4mno5";
for (var i = 0; i < 4e5; ++i)
    matchAll(string, "[0-9]");
