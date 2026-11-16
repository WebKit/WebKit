var empty = [];
noInline(Array.prototype.indexOf);

for (var i = 0; i < 1e7; ++i)
    empty.indexOf(1);
