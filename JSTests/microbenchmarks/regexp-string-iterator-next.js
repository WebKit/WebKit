// Tracks the per-iteration cost of %RegExpStringIteratorPrototype%.next, both
// through for-of over String.prototype.matchAll and through manual next() calls.

var input = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do\n"
    + "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim\n"
    + "ad minim veniam, quis nostrud exercitation ullamco laboris nisi.";

function forOfMatchAll(string, regexp)
{
    var count = 0;
    for (var match of string.matchAll(regexp))
        count += match[0].length;
    return count;
}
noInline(forOfMatchAll);

function manualNext(string, regexp)
{
    var iterator = string.matchAll(regexp);
    var count = 0;
    for (;;) {
        var result = iterator.next();
        if (result.done)
            break;
        count += result.value[0].length;
    }
    return count;
}
noInline(manualNext);

var words = /[a-z]+/g;
var withGroups = /([a-z])([a-z]+)/g;

for (var i = 0; i < 5e4; ++i) {
    forOfMatchAll(input, words);
    manualNext(input, withGroups);
}
