var findTypeForExpression = $vm.findTypeForExpression;

load("./driver/driver.js");

function wrapper()
{

var x;
var Proto = function() {};
var oldProto;
for (var i = 0; i < MaxStructureCountWithoutOverflow; i++) {
    // Make sure we get a new prototype chain on each assignment to x because objects with shared prototype chains will be merged.
    x = new Proto;
    x['field' + i] = 20;
    x = x
    oldProto = Proto;
    Proto = function() {};
    Proto.prototype.__proto__ = oldProto.prototype;
}
x = {};

var y;
Proto = function() {};
oldProto = null;
for (var i = 0; i < MaxStructureCountWithoutOverflow - 1; i++) {
    y = new Proto;
    y['field' + i] = 20;
    y = y
    oldProto = Proto;
    Proto = function() {};
    Proto.prototype.__proto__ = oldProto.prototype;
}
y = {};

}
wrapper();

var types = findTypeForExpression(wrapper, "x;"); 
assert(types.isOverflown, "x should be overflown with too many structure shapes.");

var types = findTypeForExpression(wrapper, "y;"); 
assert(!types.isOverflown, "y should not be overflown with too many structure shapes.");
