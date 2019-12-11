//@ runDefault("--useTypeProfiler=true")

var findTypeForExpression = $vm.findTypeForExpression;

function wrapper() {
    var x;
    var Proto = function() {};
    var oldProto;
    for (var i = 0; i < 100; i++) {
        // Make sure we get a new prototype chain on each assignment to x because objects with shared prototype chains will be merged.
        x = new Proto;
        x['"' + i + '"'] = 20;
        x = x
        oldProto = Proto;
        Proto = function() {};
        Proto.prototype.__proto__ = oldProto.prototype;
    }
    x = {};
}
wrapper();

var types = findTypeForExpression(wrapper, "x;"); 
JSON.stringify(types);
