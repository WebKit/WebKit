description("Tests for Array.prototype.filter");

function passUndefined(element, index, array) {
    return typeof element === "undefined";
}
function passEven(a) {
    return !(a & 1);
}
function passAfter5(element, index) {
    return index >= 5;
}
var sparseArrayLength = 10100;
mixPartialAndFast = new Array(sparseArrayLength);
mixPartialAndFast[sparseArrayLength - 1] = sparseArrayLength - 1;
for(var i = 0; i < 10; i++)
    mixPartialAndFast[i] = i;
function toObject(array) {
    var result = {};
    result.length = array.length;
    for (var i in array)
        result[i] = array[i];
    result.filter=Array.prototype.filter;
    return result;
}
function reverseInsertionOrder(array) {
    var obj = toObject(array);
    var props = [];
    for (var i in obj)
        props.push(i);
    var result = {};
    for (var i = props.length - 1; i >= 0; i--)
        result[props[i]] = obj[props[i]];
    result.filter=Array.prototype.filter;
    return result;
}
function filterLog(f) {
    return function(i,j) {
        try {
        debug([i,j,arguments[2].toString().substring(0,20)].toString());
        return f.apply(this, arguments);
        } catch(e) {
            console.error(e);
        }
    }
}

shouldBe("[undefined].filter(passUndefined)", "[undefined]");
shouldBe("(new Array(20)).filter(passUndefined)", "[]");
shouldBe("[0,1,2,3,4,5,6,7,8,9].filter(passEven)", "[0,2,4,6,8]");
shouldBe("[0,1,2,3,4,5,6,7,8,9].filter(passAfter5)", "[5,6,7,8,9]");
shouldBe("mixPartialAndFast.filter(passAfter5)", "[5,6,7,8,9,sparseArrayLength-1]");

// Generic Object
shouldBe("toObject([undefined]).filter(passUndefined)", "[undefined]");
shouldBe("toObject(new Array(20)).filter(passUndefined)", "[]");
shouldBe("toObject([0,1,2,3,4,5,6,7,8,9]).filter(passEven)", "[0,2,4,6,8]");
shouldBe("toObject([0,1,2,3,4,5,6,7,8,9]).filter(passAfter5)", "[5,6,7,8,9]");
shouldBe("toObject(mixPartialAndFast).filter(passAfter5)", "[5,6,7,8,9,sparseArrayLength-1]");

// Reversed generic Object
shouldBe("reverseInsertionOrder([undefined]).filter(passUndefined)", "[undefined]");
shouldBe("reverseInsertionOrder(new Array(20)).filter(passUndefined)", "[]");
shouldBe("reverseInsertionOrder([0,1,2,3,4,5,6,7,8,9]).filter(passEven)", "[0,2,4,6,8]");
shouldBe("reverseInsertionOrder([0,1,2,3,4,5,6,7,8,9]).filter(passAfter5)", "[5,6,7,8,9]");
shouldBe("reverseInsertionOrder(mixPartialAndFast).filter(passAfter5)", "[5,6,7,8,9,sparseArrayLength-1]");

// Log evaluation order
shouldBe("reverseInsertionOrder([undefined]).filter(filterLog(passUndefined))", "[undefined]");
shouldBe("reverseInsertionOrder(new Array(20)).filter(filterLog(passUndefined))", "[]");
shouldBe("reverseInsertionOrder([0,1,2,3,4]).filter(filterLog(passEven))", "[0,2,4]");
shouldBe("reverseInsertionOrder(mixPartialAndFast).filter(filterLog(passAfter5))", "[5,6,7,8,9,sparseArrayLength-1]");
shouldBe("([undefined]).filter(filterLog(passUndefined))", "[undefined]");
shouldBe("(new Array(20)).filter(filterLog(passUndefined))", "[]");
shouldBe("([0,1,2,3,4]).filter(filterLog(passEven))", "[0,2,4]");
shouldBe("(mixPartialAndFast).filter(filterLog(passAfter5))", "[5,6,7,8,9,sparseArrayLength-1]");

shouldBe("[1,2,3].filter(function(i,j,k,l,m){ return m=!m; })", "[1,2,3]")
successfullyParsed = true;
