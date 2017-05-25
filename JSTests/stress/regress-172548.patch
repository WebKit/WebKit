//@ run("custom-noCJIT-noConcurrentGC-noGenGC-noDFG", "--useConcurrentJIT=false", "--useConcurrentGC=false", "--useGenerationalGC=false", "--useDFGJIT=false")

// This test should not crash.

(function() {
    function foo(x, y) {
        return (( ~ (((((Math.fround(0 % y) >= y) ) / ((Math.max(((Math.atan2(((Math.pow((-Number.MAX_VALUE >>> 0), (x | 0)) | 0) >>> 0), y) >>> 0) - ( + (( ! (0 | 0)) | 0))), Math.pow(Math.fround(((x | 0) ^ y)), ( ! ( + y)))) | 0) | 0)) | 0) | 0)) | 0);
    };

    var inputs = [null, NaN, (1/0), NaN, (1/0), arguments.callee, null, (1/0), arguments.callee, NaN, (1/0), arguments.callee, (1/0), arguments.callee, NaN, null, NaN, null, (1/0), NaN, arguments.callee, (1/0), NaN, null, arguments.callee, (1/0), NaN, NaN, NaN, NaN, NaN, arguments.callee, null, (1/0), NaN, null, null, (1/0), (1/0), NaN, NaN, NaN, NaN, NaN, NaN, NaN, NaN, NaN, NaN, NaN, NaN, NaN, NaN, NaN, NaN, null, null, (1/0), NaN, null, null, arguments.callee, NaN, NaN, null, null, arguments.callee, null, NaN, (1/0), NaN, NaN, arguments.callee, NaN, arguments.callee, null, NaN, NaN, null, arguments.callee, NaN, null, (1/0), NaN, arguments.callee, null, null, NaN, null, NaN, arguments.callee, arguments.callee, arguments.callee, arguments.callee, null, arguments.callee, (1/0), (1/0), (1/0), (1/0), (1/0), NaN, (1/0), arguments.callee, NaN, (1/0)];

    for (var j = 0; j < inputs.length; ++j) {       
        for (var k = 0; k < inputs.length; ++k) {         
            foo(inputs[k], inputs[k]);
        }     
    }   

})();

Math.ceil = "\uEB0D";

m2 = new Map;
m2.toSource = (function() { });

m1 = new Map;
m2.toString();

o2 = m1.__proto__;
m2 = new Map;

test = function() {
    Math.log1p(/x/g), Math.log1p(/x/g), Math.ceil(0x100000001);
};

for (var i = 0; i < 33000; i++) {
    try {
        test();
    } catch (e) {
    }
}

o2.g0 = this;
o2.h1 = {};
