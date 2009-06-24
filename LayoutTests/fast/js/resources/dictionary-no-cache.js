description("Test to ensure that we handle caching of prototype chains containing dictionaries.");

var Test = function(){};

var methodCount = 65;

for (var i = 0; i < methodCount; i++){
    Test.prototype['myMethod' + i] = function(){};
}

var test1 = new Test();

for (var k in test1);

Test.prototype.myAdditionalMethod = function(){};
var test2 = new Test();
var j = k;
var foundNewPrototypeProperty = false;
for (var k in test2){
    if ("myAdditionalMethod" == k) foundNewPrototypeProperty = true;
}
shouldBeTrue('foundNewPrototypeProperty');

var Test = function(){};
for (var i = 0; i < methodCount; i++){
    Test.prototype['myMethod' + i] = function(){};
}
var test1 = new Test();

for (var k in test1);

delete (Test.prototype)[k]
var test2 = new Test();
var j = k;
var foundRemovedPrototypeProperty = false;
for (var k in test2){
    if (j == k) foundRemovedPrototypeProperty = true;
}
shouldBeFalse("foundRemovedPrototypeProperty");

var Test = function(){};
for (var i = 0; i < methodCount; i++){
    Test.prototype['myMethod' + i] = function(){};
}

function update(test) {
    test.newProperty = true;
}
var test1 = new Test();
update(test1);

var test2 = new Test();
update(test2);

var test3 = new Test();
update(test3);
var calledNewPrototypeSetter = false;
Test.prototype.__defineSetter__("newProperty", function(){ calledNewPrototypeSetter = true; });
var test4 = new Test();
update(test4);
shouldBeTrue('calledNewPrototypeSetter');

successfullyParsed = true;
