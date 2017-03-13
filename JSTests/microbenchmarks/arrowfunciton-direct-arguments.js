var fn = function() {
    return () => arguments[0];
}(1);

for (var i = 0; i < 100000; i++) {
    if(fn(2) !== 1) throw 0; 
}
