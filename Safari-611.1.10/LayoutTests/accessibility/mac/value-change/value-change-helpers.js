var AXTextStateChangeTypeEdit = 1;

var AXTextEditTypeDelete = 1;
var AXTextEditTypeInsert = AXTextEditTypeDelete + 1;
var AXTextEditTypeTyping = AXTextEditTypeInsert + 1;
var AXTextEditTypeDictation = AXTextEditTypeTyping + 1;
var AXTextEditTypeCut = AXTextEditTypeDictation + 1
var AXTextEditTypePaste = AXTextEditTypeCut + 1;

function stringForEditType(editType) {
    if (editType == AXTextEditTypeDelete) {
        return "Delete";
    } else if (editType == AXTextEditTypeInsert) {
        return "Insert";
    } else if (editType == AXTextEditTypeTyping) {
        return "Typing";
    } else if (editType == AXTextEditTypeDictation) {
        return "Dictation";
    } else if (editType == AXTextEditTypeCut) {
        return "Cut";
    } else if (editType == AXTextEditTypePaste) {
        return "Paste";
    }
    return "Unknown";
}

function bump(value) {
    resultIndex++;
    expectedValues[resultIndex] = value
}

function shouldBeInsert(value) {
    bump(value);
    shouldBe("actualChangeTypes[resultIndex]", "AXTextStateChangeTypeEdit");
    shouldBe("actualChangeValues[resultIndex]", "expectedValues[resultIndex]");
    shouldBe("actualEditTypes[resultIndex]", "\"Insert\"");
}

function shouldBeTyping(value) {
    bump(value);
    shouldBe("actualChangeTypes[resultIndex]", "AXTextStateChangeTypeEdit");
    shouldBe("actualChangeValues[resultIndex]", "expectedValues[resultIndex]");
    shouldBe("actualEditTypes[resultIndex]", "\"Typing\"");
}

function shouldBeDelete(value) {
    bump(value);
    shouldBe("actualChangeTypes[resultIndex]", "AXTextStateChangeTypeEdit");
    shouldBe("actualChangeValues[resultIndex]", "expectedValues[resultIndex]");
    shouldBe("actualEditTypes[resultIndex]", "\"Delete\"");
}

function shouldBeCut(value) {
    bump(value);
    shouldBe("actualChangeTypes[resultIndex]", "AXTextStateChangeTypeEdit");
    shouldBe("actualChangeValues[resultIndex]", "expectedValues[resultIndex]");
    shouldBe("actualEditTypes[resultIndex]", "\"Cut\"");
}

function shouldBePaste(value) {
    bump(value);
    shouldBe("actualChangeTypes[resultIndex]", "AXTextStateChangeTypeEdit");
    shouldBe("actualChangeValues[resultIndex]", "expectedValues[resultIndex]");
    shouldBe("actualEditTypes[resultIndex]", "\"Paste\"");
}

function shouldBeReplace(deletedValue, insertedValue) {
    bump([deletedValue, insertedValue]);
    shouldBe("actualChangeTypes[resultIndex]", "AXTextStateChangeTypeEdit");
    shouldBe("actualChangeValues[resultIndex][0]", "expectedValues[resultIndex][0]");
    shouldBe("actualEditTypes[resultIndex][0]", "\"Delete\"");
    shouldBe("actualChangeValues[resultIndex][1]", "expectedValues[resultIndex][1]");
}

function shouldBePasteReplace(deletedValue, insertedValue) {
    shouldBeReplace(deletedValue, insertedValue);
    shouldBe("actualEditTypes[resultIndex][1]", "\"Paste\"");
}

function shouldBeTypingReplace(deletedValue, insertedValue) {
    shouldBeReplace(deletedValue, insertedValue);
    shouldBe("actualEditTypes[resultIndex][1]", "\"Typing\"");
}

function shouldBeInsertReplace(deletedValue, insertedValue) {
    shouldBeReplace(deletedValue, insertedValue);
    shouldBe("actualEditTypes[resultIndex][1]", "\"Insert\"");
}