description("Test to make sure that document.all works correctly with elminated CompareEq in DFG.");

function compareFunction(a)
{
    var length = a.length;

    var aIsNull = (a == null) || (null == a);
    var aIsUndefined = (a == undefined) || (undefined == a);

    if (a == null || undefined == a)
        return { isNull: aIsNull, isUndefined: aIsUndefined, length: length };
    else
        return { isNull: aIsNull, isUndefined: aIsUndefined };
}

// Warmup with sane objects.
for (let i = 0; i < 1e4; ++i) {
    let result = compareFunction({ length: 5});
    if (result.isNull || result.isUndefined)
        debug("Failed warmup with compareFunction({ length: 5}).");

    let object = new Object;
    object.length = 1;
    result = compareFunction(object);
    if (result.isNull || result.isUndefined)
        debug("Failed warmup with compareFunction(object).");
}

let documentAll = document.all;
var documentAllCompare = compareFunction(documentAll);
shouldBeTrue("documentAllCompare.isNull");
shouldBeTrue("documentAllCompare.isUndefined");
shouldBe("documentAllCompare.length", "13");

for (let i = 0; i < 1e3; ++i) {
    let result = compareFunction({ length: 5});
    if (result.isNull || result.isUndefined)
        debug("Failed tail with compareFunction({ length: 5}).");

    result = compareFunction(documentAll);
    if (!result.isNull || !result.isUndefined)
        debug("Failed tail with compareFunction(documentAll).");

    let object = new Object;
    object.length = 1;
    result = compareFunction(object);
    if (result.isNull || result.isUndefined)
        debug("Failed tail with compareFunction(object).");
}