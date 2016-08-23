//@ runNoFTL

var set = new Set;
for (var i = 0; i < 8000; ++i) {
    set.add(i);
    set.add("" + i)
    set.add({})
    set.add(i + .5)
}

var result = 0;

set.forEach(function(k){ result += set.size; })
for (var i = 8000; i >= 0; --i) {
    set.delete(i)
    set.has("" + i)
    set.add(i + .5)
}
set.forEach(function(k){ result += set.size; })

if (result != 1600048001)
    throw "Bad result: " + result;


