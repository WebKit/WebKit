var foo = function() {
    var o = {};
    o.x = "foo";
    o.y = 1;
    
    delete o.x;
    
    o.z = 2;
    
    var result = null;
    var i = 0;
    for (var p in o) {
        if (result === null)
            result = o[p];
    }
    
    if (result !== 1)
        throw new Error("Incorrect result: " + result + " (expected 1)");
};

foo();
