description("Test ensures Reflect.set functionality with DOM objects.");

debug("CustomAccessor setters for DOM objects. These setters are treated as setters in ECMA262. Reflect.set returns true because a setter is found.");
shouldBe(`Reflect.set(document.body, "scrollTop", "#Hello")`, `true`);
shouldNotBe(`Reflect.get(document.body, "scrollTop")`, `"#Hello"`);
shouldBe(`Reflect.set(document.body, "scrollTop", 0)`, `true`);
shouldBe(`Reflect.get(document.body, "scrollTop")`, `0`);
var receiver = {};
shouldThrow(`Reflect.set(document.body, "scrollTop", "Cocoa", receiver)`);
shouldBe(`Reflect.get(document.body, "scrollTop")`, `0`);
shouldBe(`Reflect.get(receiver, "scrollTop")`, `undefined`);

debug("CustomAccessor with ReadOnly");
shouldBe(`Reflect.set(document, "compatMode", false)`, `false`);
shouldBe(`Reflect.get(document, "compatMode")`, `"BackCompat"`);
var receiver = {};
shouldBe(`Reflect.set(document, "compatMode", false, receiver)`, `false`);
shouldBe(`Reflect.get(document, "compatMode")`, `"BackCompat"`);
shouldBe(`Reflect.get(receiver, "compatMode")`, `undefined`);

debug("CustomAccessor without setter for DOM objects");
shouldBe(`Reflect.set(window.history, "length", "http://www.example.com")`, `false`);
shouldNotBe(`Reflect.get(window.history, "length")`, `"http://www.example.com"`);
var receiver = {};
shouldBe(`Reflect.set(window.history, "length", "http://www.example.com", receiver)`, `false`);
shouldNotBe(`Reflect.get(window.history, "length")`, `"http://www.example.com"`);
shouldBe(`Reflect.get(receiver, "length")`, `undefined`);

debug("CustomValue setters.");
shouldBe(`Reflect.set(window, "Event", "http://www.example.com")`, `true`);
shouldBe(`Reflect.get(window, "Event")`,  `"http://www.example.com"`);
var receiver = {};
shouldBe(`Reflect.set(window, "MessageEvent", "http://www.example.com", receiver)`, `true`);
shouldNotBe(`Reflect.get(window, "MessageEvent")`, `"http://www.example.com"`);
shouldBe(`Reflect.get(receiver, "MessageEvent")`, `"http://www.example.com"`);

debug("CustomValue setters with writable=false.");
var originalMouseEvent = MouseEvent;
shouldBe(`Reflect.defineProperty(window, "MouseEvent", {
    writable: false
})`, `true`);
shouldBe(`Reflect.set(window, "MouseEvent", "http://www.example.com")`, `false`);
shouldBe(`Reflect.get(window, "MouseEvent")`, `originalMouseEvent`);
var receiver = {};
shouldBe(`Reflect.set(window, "MouseEvent", "http://www.example.com", receiver)`, `false`);
shouldBe(`Reflect.get(window, "MouseEvent")`, `originalMouseEvent`);
shouldBe(`Reflect.get(receiver, "MouseEvent")`, `undefined`);

debug("putDelegate CustomAccessor setters for DOM objects.");
shouldBe(`Reflect.set(document.location, "hash", "#Hello")`, `true`);
shouldBe(`Reflect.get(document.location, "hash")`, `"#Hello"`);
shouldBe(`Reflect.set(document.location, "hash", "#OUT")`, `true`);
shouldBe(`Reflect.get(document.location, "hash")`, `"#OUT"`);
shouldBe(`Reflect.set(document.location, 0, "#Hello")`, `true`);
shouldBe(`Reflect.get(document.location, 0)`, `"#Hello"`);
shouldBe(`Reflect.set(document.location, 0, "#OUT")`, `true`);
shouldBe(`Reflect.get(document.location, 0)`, `"#OUT"`);
var receiver = {};
shouldThrow(`Reflect.set(document.location, "hash", "#Hello", receiver)`);
shouldBe(`Reflect.get(document.location, "hash")`, `"#OUT"`);
shouldBe(`Reflect.get(receiver, "hash")`, `undefined`);
shouldBe(`Reflect.set(document.location, 0, "#Hello", receiver)`, `true`);
shouldBe(`Reflect.get(document.location, 0)`, `"#OUT"`);
shouldBe(`Reflect.get(receiver, 0)`, `"#Hello"`);

debug("putDelegate CustomAccessor without setter for DOM objects");
shouldBe(`Reflect.set(document.location, "origin", "http://www.example.com")`, `false`);
shouldNotBe(`Reflect.get(document.location, "origin")`, `"http://www.example.com"`);
var receiver = {};
shouldBe(`Reflect.set(document.location, "origin", "http://www.example.com", receiver)`, `false`);
shouldNotBe(`Reflect.get(document.location, "origin")`, `"http://www.example.com"`);
shouldBe(`Reflect.get(receiver, "origin")`, `undefined`);

debug("NPAPI object with Reflect.set");
var npobject = document.getElementById("testPlugin");
shouldBe(`Reflect.set(npobject, "Cocoa", "Sweet")`, `true`);
shouldBe(`Reflect.get(npobject, "Cocoa")`, `"Sweet"`);
var receiver = {};
shouldBe(`Reflect.set(npobject, "Cocoa", "OK", receiver)`, `true`);
shouldBe(`Reflect.get(npobject, "Cocoa")`, `"Sweet"`);
shouldBe(`Reflect.get(receiver, "Cocoa")`, `"OK"`);

shouldBe(`Reflect.defineProperty(npobject, "Cocoa", {
    writable: false
})`, `true`);
shouldBe(`Reflect.set(npobject, "Cocoa", "Hello")`, `false`);
shouldBe(`Reflect.get(npobject, "Cocoa")`, `"Sweet"`);
var receiver = {};
shouldBe(`Reflect.set(npobject, "Cocoa", "OK", receiver)`, `false`);
shouldBe(`Reflect.get(npobject, "Cocoa")`, `"Sweet"`);
shouldBe(`Reflect.get(receiver, "Cocoa")`, `undefined`);

shouldBe(`Reflect.set(npobject, 0, "Sweet")`, `true`);
shouldBe(`Reflect.get(npobject, 0)`, `"Sweet"`);
var receiver = {};
shouldBe(`Reflect.set(npobject, 0, "OK", receiver)`, `true`);
shouldBe(`Reflect.get(npobject, 0)`, `"Sweet"`);
shouldBe(`Reflect.get(receiver, 0)`, `"OK"`);

shouldBe(`Reflect.defineProperty(npobject, 0, {
    writable: false
})`, `true`);
shouldBe(`Reflect.set(npobject, 0, "Hello")`, `false`);
shouldBe(`Reflect.get(npobject, 0)`, `"Sweet"`);
var receiver = {};
shouldBe(`Reflect.set(npobject, 0, "OK", receiver)`, `false`);
shouldBe(`Reflect.get(npobject, 0)`, `"Sweet"`);
shouldBe(`Reflect.get(receiver, 0)`, `undefined`);

debug("DOMStringMap");
document.body.setAttribute("data-cocoa", "cappuccino");
shouldBe(`Reflect.get(document.body.dataset, "cocoa")`, `"cappuccino"`);
shouldBe(`Reflect.set(document.body.dataset, "cocoa", "sweet")`, `true`);
shouldBe(`Reflect.get(document.body.dataset, "cocoa")`, `"sweet"`);
shouldThrow(`Reflect.set(document.body.dataset, "cocoa-cappuccino", "sweet")`);
shouldBe(`Reflect.set(document.body.dataset, 0, "sweet")`, `true`);
shouldBe(`Reflect.get(document.body.dataset, 0)`, `"sweet"`);

debug("DOMStringMap ignores the receiver. These putDelegate only work with ::put (not ::defineOwnProperty). So they behave as the special setter, we should not fallback to the OrdinarySet.");
var receiver = {};
shouldBe(`Reflect.set(document.body.dataset, "cocoa", "ok", receiver)`, `true`);
shouldBe(`Reflect.get(document.body.dataset, "cocoa")`, `"ok"`);
shouldBe(`Reflect.get(receiver, "cocoa")`, `undefined`);
var receiver = {};
shouldBe(`Reflect.set(document.body.dataset, 0, "ok", receiver)`, `true`);
shouldBe(`Reflect.get(document.body.dataset, 0)`, `"ok"`);
shouldBe(`Reflect.get(receiver, 0)`, `undefined`);

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

debug("CustomIndexedSetter ignores the receiver. These methods only work with ::put (not ::defineOwnProperty). So they behave as the special setter, we should not fallback to the OrdinarySet.");
var option3 = document.createElement("option");
option3.value = "rabbit";
option3.textContent = "Rabbit";
var receiver = {};
shouldBe(`Reflect.set(select, 0, option3, receiver)`, `true`);
shouldBe(`Reflect.get(receiver, 0)`, `undefined`);
shouldBe(`Reflect.get(select, 0)`, `option3`);
