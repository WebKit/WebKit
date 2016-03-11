description("Test ensures Reflect.set functionality with DOM objects.");

debug("CustomAccessor setters for DOM objects. These setters are treated as setters in ECMA262. Reflect.set returns true because a setter is found.");
shouldBe(`Reflect.set(document.body, "scrollTop", "#Hello")`, `true`);
shouldNotBe(`Reflect.get(document.body, "scrollTop")`, `"#Hello"`);
shouldBe(`Reflect.set(document.body, "scrollTop", 0)`, `true`);
shouldBe(`Reflect.get(document.body, "scrollTop")`, `0`);

debug("CustomAccessor with ReadOnly");
shouldBe(`Reflect.set(document, "compatMode", false)`, `false`);
shouldBe(`Reflect.get(document, "compatMode")`, `"BackCompat"`);

debug("CustomAccessor without setter for DOM objects");
shouldBe(`Reflect.set(window.history, "length", "http://www.example.com")`, `false`);
shouldNotBe(`Reflect.get(window.history, "length")`, `"http://www.example.com"`);

debug("CustomValue setters.");
shouldBe(`Reflect.set(window, "Event", "http://www.example.com")`, `true`);
shouldBe(`Reflect.get(window, "Event")`,  `"http://www.example.com"`);

debug("CustomValue setters with writable=false.");
var originalMouseEvent = MouseEvent;
shouldBe(`Reflect.defineProperty(window, "MouseEvent", {
    writable: false
})`, `true`);
shouldBe(`Reflect.set(window, "MouseEvent", "http://www.example.com")`, `false`);
shouldBe(`Reflect.get(window, "MouseEvent")`, `originalMouseEvent`);

debug("putDelegate CustomAccessor setters for DOM objects.");
shouldBe(`Reflect.set(document.location, "hash", "#Hello")`, `true`);
shouldBe(`Reflect.get(document.location, "hash")`, `"#Hello"`);
shouldBe(`Reflect.set(document.location, "hash", "#OUT")`, `true`);
shouldBe(`Reflect.get(document.location, "hash")`, `"#OUT"`);
shouldBe(`Reflect.set(document.location, 0, "#Hello")`, `true`);
shouldBe(`Reflect.get(document.location, 0)`, `"#Hello"`);
shouldBe(`Reflect.set(document.location, 0, "#OUT")`, `true`);
shouldBe(`Reflect.get(document.location, 0)`, `"#OUT"`);

debug("putDelegate CustomAccessor without setter for DOM objects");
shouldBe(`Reflect.set(document.location, "origin", "http://www.example.com")`, `false`);

debug("NPAPI object with Reflect.set");
var npobject = document.getElementById("testPlugin");
shouldBe(`Reflect.set(npobject, "Cocoa", "Sweet")`, `true`);
shouldBe(`Reflect.get(npobject, "Cocoa")`, `"Sweet"`);
shouldBe(`Reflect.defineProperty(npobject, "Cocoa", {
    writable: false
})`, `true`);
shouldBe(`Reflect.set(npobject, "Cocoa", "Hello")`, `false`);
shouldBe(`Reflect.get(npobject, "Cocoa")`, `"Sweet"`);
shouldBe(`Reflect.set(npobject, 0, "Sweet")`, `true`);
shouldBe(`Reflect.get(npobject, 0)`, `"Sweet"`);
shouldBe(`Reflect.defineProperty(npobject, 0, {
    writable: false
})`, `true`);
shouldBe(`Reflect.set(npobject, 0, "Hello")`, `false`);
shouldBe(`Reflect.get(npobject, 0)`, `"Sweet"`);

debug("DOMStringMap");
document.body.setAttribute("data-cocoa", "cappuccino");
shouldBe(`Reflect.get(document.body.dataset, "cocoa")`, `"cappuccino"`);
shouldBe(`Reflect.set(document.body.dataset, "cocoa", "sweet")`, `true`);
shouldBe(`Reflect.get(document.body.dataset, "cocoa")`, `"sweet"`);
shouldThrow(`Reflect.set(document.body.dataset, "cocoa-cappuccino", "sweet")`);
shouldBe(`Reflect.set(document.body.dataset, 0, "sweet")`, `true`);
shouldBe(`Reflect.get(document.body.dataset, 0)`, `"sweet"`);

debug("CustomIndexedSetter");
var select = document.getElementById("testSelect");
shouldBe(`Reflect.get(select, 0).value`, `"cocoa"`);
shouldBe(`Reflect.get(select, "length")`, `3`);
var option = document.createElement("option");
option.value = "mocha";
option.textContent = "Mocha";
shouldBe(`Reflect.set(select, 0, option)`, `true`);
shouldBe(`Reflect.get(select, 0).value`, `"mocha"`);
shouldBe(`Reflect.get(select, "length")`, `3`);
shouldBe(`Reflect.set(select, 42, option)`, `true`);
shouldBe(`Reflect.get(select, "length")`, `42`);  // Update validity prevent us to insert duplicate options.

var option2 = document.createElement("option");
option2.value = "kilimanjaro";
option2.textContent = "Kilimanjaro";
shouldBe(`Reflect.set(select, 44, option2)`, `true`);
shouldBe(`Reflect.get(select, 44).value`, `"kilimanjaro"`);
shouldThrow(`Reflect.set(select, 20, "Kilimanjaro")`);
