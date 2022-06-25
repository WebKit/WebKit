var entries = Set.prototype.entries;

function test(set)
{
    entries.call(set);
}
noInline(test);

var set = new Set();
for (var i = 0; i < 1e6; ++i) {
    test(set);
}
var array = [];
for (var i = 0; i < 1e3; ++i) {
    try {
        test(array);
    } catch {
    }
}
