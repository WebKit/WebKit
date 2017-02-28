var fn = function() {
    var args = arguments;
    return () => args[0];
}(1);

noInline(fn);

for (var i = 0; i < 10000; i++) {
    if(fn(2) !== 1) throw 0;
}