(function() {
    var result = 0;
    var d = new DataView(new ArrayBuffer(5));
    for (var i = 0; i < 1000000; i++) {
        d.setInt8(0, 4);
        d.setInt8(1, 2);
        d.setInt8(2, 6);
        d.setInt16(0, 20);
        result += d.getInt8(2) + d.getInt8(0);
    }
    if (result != 6000000) 
        throw "Bad result: " + result;
})();
