var empty = new Int8Array();
noInline(Int8Array.prototype.lastIndexOf);

for (var i = 0; i < 1e7; ++i)
    empty.lastIndexOf(1);
