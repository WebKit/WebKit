var keys = [];
for (var i = 0; i < 40; ++i) {
    var name = "";
    for (var j = 0, x = i * 2654435761; j < 6; ++j, x = (x * 48271 + 12345) | 0)
        name += String.fromCharCode(97 + ((x >>> 8) % 26));
    keys.push(name + (i % 3 ? "Id" : "Name"));
}
for (var i = 0; i < 5e4; ++i)
    keys.slice().sort();
