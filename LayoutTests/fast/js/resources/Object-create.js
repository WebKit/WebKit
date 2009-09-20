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
successfullyParsed = true;
