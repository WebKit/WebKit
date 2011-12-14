description("Test to ensure correct behaviour of replacer functions in JSON.stringify");

var object = {0:0, 1:1, 2:2, 3:undefined};
var array = [0, 1, 2, undefined];
function returnUndefined(){}
function returnObjectFor1(k, v) {
    if (k == "1")
        return {};
    return v;
}
function returnArrayFor1(k, v) {
    if (k == "1")
        return [];
    return v;
}
function returnUndefinedFor1(k, v) {
    if (k == "1")
        return undefined;
    return v;
}
function returnNullFor1(k, v) {
    if (k == "1")
        return null;
    return v;
}
function returnCycleObjectFor1(k, v) {
    if (k == "1")
        return object;
    return v;
}
function returnCycleArrayFor1(k, v) {
    if (k == "1")
        return array;
    return v;
}
function returnFunctionFor1(k, v) {
    if (k == "1")
        return function(){};
    return v;
}
function returnStringForUndefined(k, v) {
    if (v === undefined)
        return "undefined value";
    return v;
}

shouldBeUndefined("JSON.stringify(object, returnUndefined)");
shouldBeUndefined("JSON.stringify(array, returnUndefined)");

shouldBe("JSON.stringify(object, returnObjectFor1)", '\'{"0":0,"1":{},"2":2}\'');
shouldBe("JSON.stringify(array, returnObjectFor1)", '\'[0,{},2,null]\'');

shouldBe("JSON.stringify(object, returnArrayFor1)", '\'{"0":0,"1":[],"2":2}\'');
shouldBe("JSON.stringify(array, returnArrayFor1)", '\'[0,[],2,null]\'');

shouldBe("JSON.stringify(object, returnUndefinedFor1)", '\'{"0":0,"2":2}\'');
shouldBe("JSON.stringify(array, returnUndefinedFor1)", '\'[0,null,2,null]\'');

shouldBe("JSON.stringify(object, returnFunctionFor1)", '\'{"0":0,"2":2}\'');
shouldBe("JSON.stringify(array, returnFunctionFor1)", '\'[0,null,2,null]\'');

shouldBe("JSON.stringify(object, returnNullFor1)", '\'{"0":0,"1":null,"2":2}\'');
shouldBe("JSON.stringify(array, returnNullFor1)", '\'[0,null,2,null]\'');

shouldBe("JSON.stringify(object, returnStringForUndefined)", '\'{"0":0,"1":1,"2":2,"3":"undefined value"}\'');
shouldBe("JSON.stringify(array, returnStringForUndefined)", '\'[0,1,2,"undefined value"]\'');

shouldThrow("JSON.stringify(object, returnCycleObjectFor1)");
shouldThrow("JSON.stringify(array, returnCycleObjectFor1)");

shouldThrow("JSON.stringify(object, returnCycleArrayFor1)");
shouldThrow("JSON.stringify(array, returnCycleArrayFor1)");

successfullyParsed = true;
