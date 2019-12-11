var counter = 0;
function keys(a, b, c) {
    for (var i in a) {
        for (var j in b) {
            for (var k in c) {
            }
        }
    }
    if ((++counter) % 1000 === 0)
        gc();
}
noInline(keys);

var dictionary = {
    0: 2,
    "Hey": "Hello",
    "World": 32.4,
    "deleted": 20,
};
delete dictionary["deleted"];
for (var i = 0; i < 1e4; ++i) {
    keys([], [20], ["Hey"]);
    keys(["OK", 30], { Hello: 0, World: 2 }, []);
    keys(["OK", 30], { Hello: 0, World: 2 }, [42]);
    keys(["OK", 30], [], { Hello: 0, World: 2 });
    keys(["OK", 30], [2.5, 3.7], dictionary);
    keys(dictionary, { Hello: 0, World: 2 }, dictionary);
    keys({ Hello: 0, World: 2 }, dictionary, { Hello: 0, World: 2, 3:42 });
}
