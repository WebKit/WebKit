noInline(Array.prototype.shift);
noInline(Array.prototype.unshift);

for (var i = 0; i < 1e6; ++i) {
    var arr = [];
    arr.shift();
    arr.unshift();
}
