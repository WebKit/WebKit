description("Test to ensure correct behaviour of Object.defineProperties");

shouldThrow("Object.create()");
shouldThrow("Object.create('a string')");
shouldThrow("Object.create({}, 'a string')");
shouldThrow("Object.create(null, 'a string')");
shouldBe("JSON.stringify(Object.create(null,{property:{value:'foo', enumerable:true}, property2:{value:'foo', enumerable:true}}))", '\'{"property":"foo","property2":"foo"}\'');
shouldBe("JSON.stringify(Object.create({},{property:{value:'foo', enumerable:true}, property2:{value:'foo', enumerable:true}}))", '\'{"property":"foo","property2":"foo"}\'');
shouldBe("JSON.stringify(Object.create({},{property:{value:'foo'}, property2:{value:'foo', enumerable:true}}))", '\'{"property2":"foo"}\'');
shouldBe("JSON.stringify(Object.create(null,{property:{value:'foo'}, property2:{value:'foo', enumerable:true}}))", '\'{"property2":"foo"}\'');
shouldBe("Object.getPrototypeOf(Object.create(Array.prototype))", "Array.prototype");
shouldBe("Object.getPrototypeOf(Object.create(null))", "null");
function valueGet() { return true; }
var DescriptorWithValueGetter = { foo: Object.create(null, { value: { get: valueGet }})};
var DescriptorWithEnumerableGetter = { foo: Object.create(null, { value: {value: true}, enumerable: { get: valueGet }})};
var DescriptorWithConfigurableGetter = { foo: Object.create(null, { value: {value: true}, configurable: { get: valueGet }})};
var DescriptorWithWritableGetter = { foo: Object.create(null, { value: {value: true}, writable: { get: valueGet }})};
var DescriptorWithGetGetter = { foo: Object.create(null, { get: { get: function() { return valueGet } }})};
var DescriptorWithSetGetter = { foo: Object.create(null, { get: { value: valueGet}, set: { get: function(){ return valueGet; } }})};
shouldBeTrue("Object.create(null, DescriptorWithValueGetter).foo");
shouldBeTrue("Object.create(null, DescriptorWithEnumerableGetter).foo");
shouldBeTrue("Object.create(null, DescriptorWithConfigurableGetter).foo");
shouldBeTrue("Object.create(null, DescriptorWithWritableGetter).foo");
shouldBeTrue("Object.create(null, DescriptorWithGetGetter).foo");
shouldBeTrue("Object.create(null, DescriptorWithSetGetter).foo");
