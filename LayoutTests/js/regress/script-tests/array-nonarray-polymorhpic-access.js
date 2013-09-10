//@ runDefault

var f = function(a) {
    var sum = 0;
    for (var i = 0; i < a.length; i++) {
        sum += a[i];
    }   
    return sum;
};

var run = function() {
    var o1 = []; 
    for (var i = 0; i < 100; i++) {
        o1[i] = i;
    }
    
    var o2 = {}; 
    for (var i = 0; i < o1.length; i++) {
        o2[i] = o1[i];
    }
    o2.length = o1.length;

    var sum = 0;
    for (var i = 0; i < 100000; i++) {
        if (i % 2 === 0)
            sum += f(o1);
        else
            sum += f(o2);
    }
    return sum;
};

var result = run();
if (result !== 495000000)
    throw "Bad result";
