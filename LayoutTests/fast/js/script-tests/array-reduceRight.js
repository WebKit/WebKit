description(
"This test checks the behavior of the reduceRight() method on a number of objects."
);

function toObject(array) {
    var o = {};
    for (var i in array)
        o[i] = array[i];
    o.length = array.length;
    o.reduceRight = Array.prototype.reduceRight;
    return o;
}
function toUnorderedObject(array) {
    var o = {};
    var props = [];
    for (var i in array)
        props.push(i);
    for (var i = props.length - 1; i >= 0; i--)
        o[props[i]] = array[props[i]];
    o.length = array.length;
    o.reduceRight = Array.prototype.reduceRight;
    return o;
}

shouldBe("[0,1,2,3].reduceRight(function(a,b){ return a + b; })", "6");
shouldBe("[1,2,3].reduceRight(function(a,b){ return a + b; })", "6");
shouldBe("[0,1,2,3].reduceRight(function(a,b){ return a + b; }, 4)", "10");
shouldBe("[1,2,3].reduceRight(function(a,b){ return a + b; }, 4)", "10");
shouldBe("toObject([0,1,2,3]).reduceRight(function(a,b){ return a + b; })", "6");
shouldBe("toObject([1,2,3]).reduceRight(function(a,b){ return a + b; })", "6");
shouldBe("toObject([0,1,2,3]).reduceRight(function(a,b){ return a + b; }, 4)", "10");
shouldBe("toObject([1,2,3]).reduceRight(function(a,b){ return a + b; }, 4)", "10");
shouldBe("toUnorderedObject([0,1,2,3]).reduceRight(function(a,b){ return a + b; })", "6");
shouldBe("toUnorderedObject([1,2,3]).reduceRight(function(a,b){ return a + b; })", "6");
shouldBe("toUnorderedObject([0,1,2,3]).reduceRight(function(a,b){ return a + b; }, 4)", "10");
shouldBe("toUnorderedObject([1,2,3]).reduceRight(function(a,b){ return a + b; }, 4)", "10");
sparseArray = [];
sparseArray[10] = 10;
shouldBe("sparseArray.reduceRight(function(a,b){ return a + b; }, 0)", "10");
shouldBe("toObject(sparseArray).reduceRight(function(a,b){ return a + b; }, 0)", "10");
var callCount = 0;
shouldBe("sparseArray.reduceRight(function(a,b){ callCount++; }); callCount", "0");
callCount = 0;
shouldBe("toObject(sparseArray).reduceRight(function(a,b){ callCount++; }); callCount", "0");
var callCount = 0;
shouldBe("sparseArray.reduceRight(function(a,b){ callCount++; }, 0); callCount", "1");
callCount = 0;
shouldBe("toObject(sparseArray).reduceRight(function(a,b){ callCount++; }, 0); callCount", "1");
callCount = 0;
shouldBe("[0,1,2,3,4].reduceRight(function(a,b){ callCount++; }, 0); callCount", "5");
callCount = 0;
shouldBe("[0,1,2,3,4].reduceRight(function(a,b){ callCount++; }); callCount", "4");
callCount = 0;
shouldBe("[1, 2, 3, 4].reduceRight(function(a,b, i, thisObj){ thisObj.length--; callCount++; return a + b; }, 0); callCount", "4");
callCount = 0;
shouldBe("[1, 2, 3, 4].reduceRight(function(a,b, i, thisObj){ thisObj.length = 1; callCount++; return a + b; }, 0); callCount", "2");
callCount = 0;
shouldBe("[1, 2, 3, 4].reduceRight(function(a,b, i, thisObj){ thisObj.length++; callCount++; return a + b; }, 0); callCount", "4");
callCount = 0;
shouldBe("toObject([1, 2, 3, 4]).reduceRight(function(a,b, i, thisObj){ thisObj.length--; callCount++; return a + b; }, 0); callCount", "4");
callCount = 0;
shouldBe("toObject([1, 2, 3, 4]).reduceRight(function(a,b, i, thisObj){ thisObj.length++; callCount++; return a + b; }, 0); callCount", "4");

shouldBe("[[0,1], [2,3], [4,5]].reduceRight(function(a,b) {return a.concat(b);}, [])", "[4,5,2,3,0,1]");
shouldBe("toObject([[0,1], [2,3], [4,5]]).reduceRight(function(a,b) {return a.concat(b);}, [])", "[4,5,2,3,0,1]");
shouldBe("toObject([0,1,2,3,4,5]).reduceRight(function(a,b,i) {return a.concat([i,b]);}, [])", "[5,5,4,4,3,3,2,2,1,1,0,0]");
shouldBe("toUnorderedObject([[0,1], [2,3], [4,5]]).reduceRight(function(a,b) {return a.concat(b);}, [])", "[4,5,2,3,0,1]");
shouldBe("toUnorderedObject([0,1,2,3,4,5]).reduceRight(function(a,b,i) {return a.concat([i,b]);}, [])", "[5,5,4,4,3,3,2,2,1,1,0,0]");
shouldBe("[0,1,2,3,4,5].reduceRight(function(a,b,i) {return a.concat([i,b]);}, [])", "[5,5,4,4,3,3,2,2,1,1,0,0]");
successfullyParsed = true;
