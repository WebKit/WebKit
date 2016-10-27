description(
"Tests that check that sloppy getters and setters on the global object don't coerce undefined to their this."
);

var act_e = undefined;
try { 
    this.__proto__;
    var originalProto = this.__proto__;
    this.__proto__ = 1;
    if (this.__proto__ != originalProto) 
        throw "__proto__ was modified";
} catch (e) {
    act_e = e;
}

if (act_e) 
    testFailed("shouldn't have thrown '"+ e + "' when accessing and modifying this.__proto__");
else 
    testPassed("this.__proto__ accessed succesfully and stayed frozen.");

shouldNotThrow("Object.prototype.valueOf.call(3);");
shouldThrow("Object.prototype.valueOf.call(null);");


shouldNotThrow("Object.getOwnPropertyDescriptor(Object.prototype,'__proto__').get()");
shouldNotThrow("Object.getOwnPropertyDescriptor(Object.prototype,'__proto__').set(['foo'])");

shouldThrow("(0,Object.getOwnPropertyDescriptor(Object.prototype,'__proto__').get)()", "\"TypeError: Object.prototype.__proto__ called on null or undefined\"");
shouldThrow("(0,Object.getOwnPropertyDescriptor(Object.prototype,'__proto__').set)(['foo'])", "\"TypeError: Object.prototype.__proto__ called on null or undefined\"");


var top_level_sloppy_getter = Object.getOwnPropertyDescriptor(Object.prototype,'__proto__').get;
shouldThrow("top_level_sloppy_getter();", "\"TypeError: Object.prototype.__proto__ called on null or undefined\"");

var top_level_sloppy_setter = Object.getOwnPropertyDescriptor(Object.prototype,'__proto__').set;
shouldThrow("top_level_sloppy_setter(['foo']);", "\"TypeError: Object.prototype.__proto__ called on null or undefined\"");
