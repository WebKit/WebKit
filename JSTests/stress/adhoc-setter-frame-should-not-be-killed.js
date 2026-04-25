let o = {};
o.__defineSetter__('f', function(a) {
    arguments = a;
});
for (let i = 0; i<testLoopCount; i++) {
    o.f = 0
}
