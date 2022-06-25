var entries = Map.prototype.entries;

function test(map)
{
    entries.call(map);
}
noInline(test);

var map = new Map();
for (var i = 0; i < 1e6; ++i) {
    test(map);
}
var array = [];
for (var i = 0; i < 1e3; ++i) {
    try {
        test(array);
    } catch {
    }
}
