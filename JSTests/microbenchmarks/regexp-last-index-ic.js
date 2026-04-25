function load(reg) {
    return reg.lastIndex;
}
noInline(load);

function store(reg, value) {
    reg.lastIndex = value;
}
noInline(store);

var reg = /test/i
for (var i = 0; i < 2e6; ++i) {
    load(reg);
    store(reg, i);
}
