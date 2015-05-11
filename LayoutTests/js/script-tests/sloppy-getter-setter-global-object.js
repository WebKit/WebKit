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

shouldThrow("(0,Object.getOwnPropertyDescriptor(Object.prototype,'__proto__').get)()", "\"TypeError: Can't convert undefined or null to object\"");
shouldThrow("(0,Object.getOwnPropertyDescriptor(Object.prototype,'__proto__').set)(['foo'])", "\"TypeError: Can't convert undefined or null to object\"");


var top_level_sloppy_getter = Object.getOwnPropertyDescriptor(Object.prototype,'__proto__').get;
shouldNotThrow("top_level_sloppy_getter();");

var top_level_sloppy_setter = Object.getOwnPropertyDescriptor(Object.prototype,'__proto__').set;
shouldNotThrow("top_level_sloppy_setter(['foo']);");
