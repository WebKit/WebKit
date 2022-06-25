var empty = new Int8Array();
noInline(Int8Array.prototype.indexOf);

for (var i = 0; i < 1e7; ++i)
    empty.indexOf(1);
