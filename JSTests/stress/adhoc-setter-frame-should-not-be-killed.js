let o = {};
o.__defineSetter__('f', function(a) {
    arguments = a;
});
for (let i = 0; i<1000000; i++) {
    o.f = 0
}
