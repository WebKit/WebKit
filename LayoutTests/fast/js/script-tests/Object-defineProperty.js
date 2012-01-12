description("Test to ensure correct behaviour of Object.defineProperty");

shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {}), 'foo'))",
         "JSON.stringify({writable: false, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {get:undefined}), 'foo'))",
         "JSON.stringify({enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, writable: false}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, writable: true}), 'foo'))",
         "JSON.stringify({value: 1, writable: true, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, enumerable: false}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, enumerable: true}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: true, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, configurable: false}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: false})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty({}, 'foo', {value:1, configurable: true}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: true})");
shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty([1,2,3], 'foo', {value:1, configurable: true}), 'foo'))",
         "JSON.stringify({value: 1, writable: false, enumerable: false, configurable: true})");
shouldBe("Object.getOwnPropertyDescriptor(Object.defineProperty([1,2,3], '1', {value:'foo', configurable: true}), '1').value", "'foo'");
shouldBe("a=[1,2,3], Object.defineProperty(a, '1', {value:'foo', configurable: true}), a[1]", "'foo'");
shouldBe("Object.getOwnPropertyDescriptor(Object.defineProperty([1,2,3], '1', {get:getter, configurable: true}), '1').get", "getter");
shouldThrow("Object.defineProperty([1,2,3], '1', {get:getter, configurable: true})[1]", "'called getter'");

shouldThrow("Object.defineProperty()");
shouldThrow("Object.defineProperty(null)");
shouldThrow("Object.defineProperty('foo')");
shouldThrow("Object.defineProperty({})");
shouldThrow("Object.defineProperty({}, 'foo')");
shouldThrow("Object.defineProperty({}, 'foo', {get:undefined, value:true}).foo");
shouldBeTrue("Object.defineProperty({get foo() { return true; } }, 'foo', {configurable:false}).foo");

function createUnconfigurableProperty(o, prop, writable, enumerable) {
    writable = writable || false;
    enumerable = enumerable || false;
    return Object.defineProperty(o, prop, {value:1, configurable:false, writable: writable, enumerable: enumerable});
}
shouldThrow("Object.defineProperty(createUnconfigurableProperty({}, 'foo'), 'foo', {configurable: true})");
shouldThrow("Object.defineProperty(createUnconfigurableProperty({}, 'foo'), 'foo', {writable: true})");
shouldThrow("Object.defineProperty(createUnconfigurableProperty({}, 'foo'), 'foo', {enumerable: true})");
shouldThrow("Object.defineProperty(createUnconfigurableProperty({}, 'foo', false, true), 'foo', {enumerable: false}), 'foo'");

shouldBe("JSON.stringify(Object.getOwnPropertyDescriptor(Object.defineProperty(createUnconfigurableProperty({}, 'foo', true), 'foo', {writable: false}), 'foo'))",
         "JSON.stringify({value: 1, writable: true, enumerable: false, configurable: false})");

shouldThrow("Object.defineProperty({}, 'foo', {value:1, get: function(){}})");
shouldThrow("Object.defineProperty({}, 'foo', {value:1, set: function(){}})");
shouldThrow("Object.defineProperty({}, 'foo', {writable:true, get: function(){}})");
shouldThrow("Object.defineProperty({}, 'foo', {writable:true, set: function(){}})");
shouldThrow("Object.defineProperty({}, 'foo', {get: null})");
shouldThrow("Object.defineProperty({}, 'foo', {set: null})");
function getter(){ throw "called getter"; }
function getter1(){ throw "called getter1"; }
function setter(){ throw "called setter"; }
function setter1(){ throw "called setter1"; }
shouldThrow("Object.defineProperty({}, 'foo', {set: setter}).foo='test'", "'called setter'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter}), 'foo', {set: setter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter}), 'foo', {get: getter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter}), 'foo', {value: 1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter, configurable: true}), 'foo', {set: setter1}).foo='test'", "'called setter1'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter, configurable: true}), 'foo', {get: getter1}).foo", "'called getter1'");
shouldBeTrue("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter, configurable: true}), 'foo', {value: true}).foo");
shouldBeTrue("'foo' in Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter, configurable: true}), 'foo', {writable: true})");
shouldBeUndefined("Object.defineProperty(Object.defineProperty({}, 'foo', {set: setter, configurable: true}), 'foo', {writable: true}).foo");
shouldThrow("Object.defineProperty({}, 'foo', {get: getter}).foo", "'called getter'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter}), 'foo', {get: getter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter}), 'foo', {set: setter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter, configurable: true}), 'foo', {get: getter1}).foo", "'called getter1'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter, configurable: true}), 'foo', {set: setter1}).foo='test'", "'called setter1'");
shouldBeTrue("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter, configurable: true}), 'foo', {value: true}).foo");
shouldBeUndefined("Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter, configurable: true}), 'foo', {writable: true}).foo");
shouldBeTrue("'foo' in Object.defineProperty(Object.defineProperty({}, 'foo', {get: getter, configurable: true}), 'foo', {writable: true})");

shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1}), 'foo', {set: setter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1, configurable: true}), 'foo', {set: setter1}).foo='test'", "'called setter1'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1}), 'foo', {get: getter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1}), 'foo', {set: setter1})");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1, configurable: true}), 'foo', {get: getter1}).foo", "'called getter1'");
shouldThrow("Object.defineProperty(Object.defineProperty({}, 'foo', {value: 1, configurable: true}), 'foo', {set: setter1}).foo='test'", "'called setter1'");

// Test an object with a getter setter.
// Either accessor may be omitted or replaced with undefined, or both may be replaced with undefined.
shouldBe("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}); o.foo", '42')
shouldBe("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}); o.foo = 42; o.result;", '42');
shouldBe("var o = Object.defineProperty({}, 'foo', {get:undefined, set:function(x){this.result = x;}}); o.foo", 'undefined')
shouldBe("var o = Object.defineProperty({}, 'foo', {get:undefined, set:function(x){this.result = x;}}); o.foo = 42; o.result;", '42');
shouldBe("var o = Object.defineProperty({}, 'foo', {set:function(x){this.result = x;}}); o.foo", 'undefined')
shouldBe("var o = Object.defineProperty({}, 'foo', {set:function(x){this.result = x;}}); o.foo = 42; o.result;", '42');
shouldBe("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:undefined}); o.foo", '42')
shouldThrow("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}, set:undefined}); o.foo = 42; o.result;");
shouldBe("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}}); o.foo", '42')
shouldThrow("var o = Object.defineProperty({}, 'foo', {get:function(){return 42;}}); o.foo = 42; o.result;");
shouldBe("var o = Object.defineProperty({}, 'foo', {get:undefined, set:undefined}); o.foo", 'undefined')
shouldThrow("var o = Object.defineProperty({}, 'foo', {get:undefined, set:undefined}); o.foo = 42; o.result;");
// Test replacing or removing either the getter or setter individually.
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:function(){return 13;}}); o.foo", '13')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:function(){return 13;}}); o.foo = 42; o.result;", '42')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:undefined}); o.foo", 'undefined')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {get:undefined}); o.foo = 42; o.result;", '42')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:function(){this.result = 13;}}); o.foo", '42')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:function(){this.result = 13;}}); o.foo = 42; o.result;", '13')
shouldBe("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:undefined}); o.foo", '42')
shouldThrow("var o = Object.defineProperty(Object.defineProperty({foo:1}, 'foo', {get:function(){return 42;}, set:function(x){this.result = x;}}), 'foo', {set:undefined}); o.foo = 42; o.result;")
