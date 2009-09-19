description("Test to ensure correct behaviour of Object.defineProperties");

shouldThrow("Object.defineProperties()");
shouldThrow("Object.defineProperties('a string')");
shouldThrow("Object.defineProperties({}, 'a string')");
shouldBe("JSON.stringify(Object.defineProperties({},{property:{value:'foo', enumerable:true}, property2:{value:'foo', enumerable:true}}))", '\'{"property":"foo","property2":"foo"}\'');
shouldBe("JSON.stringify(Object.defineProperties({},{property:{value:'foo'}, property2:{value:'foo', enumerable:true}}))", '\'{"property2":"foo"}\'');
shouldBe("JSON.stringify(Object.defineProperties({property:'foo'},{property:{value:'foo', enumerable:true}, property2:{value:'foo', enumerable:true}}))", '\'{"property":"foo","property2":"foo"}\'');
shouldBe("JSON.stringify(Object.defineProperties({property:'foo'},{property:{value:'foo', enumerable:false}, property2:{value:'foo', enumerable:true}}))", '\'{"property2":"foo"}\'');
shouldBe("JSON.stringify(Object.defineProperties({property:'foo'},{property:{value:'foo'}, property2:{value:'foo', enumerable:true}}))", '\'{"property":"foo","property2":"foo"}\'');
emptyObject={};
shouldThrow("Object.defineProperties(emptyObject, {foo:{value: true}, bar:{get:function(){}, writable:true}})");
shouldBeFalse("'foo' in emptyObject");

successfullyParsed = true;
