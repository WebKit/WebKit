importScripts('../../../resources/js-test-pre.js');

description("Tests which types are valid for crypto.randomValues.");

shouldBe("'crypto' in self", "true");
shouldBe("'getRandomValues' in self.crypto", "true");

function checkIntegerTypes() {
    var integerTypes = [
        "Uint8Array", "Int8Array", "Uint8ClampedArray",
        "Uint16Array", "Int16Array",
        "Uint32Array", "Int32Array",
    ];
    integerTypes.forEach(function(arrayType) {
        shouldBeDefined("random = crypto.getRandomValues(new "+arrayType+"(3))");
        shouldBeType("random", arrayType);

        shouldBeDefined("view = new "+arrayType+"(3)");
        shouldBeDefined("random = crypto.getRandomValues(view)");
        shouldBe("random", "view");
    });
}

function checkNonIntegerTypes() {
    var floatTypes = [
        "Float32Array", "Float64Array",
    ];
    floatTypes.forEach(function(arrayType) {
        shouldThrow("crypto.getRandomValues(new "+arrayType+"(3))");
    });

    shouldBeDefined("buffer = new Uint8Array(32)");
    shouldBeDefined("buffer.buffer");
    shouldBeDefined("view = new DataView(buffer.buffer)");
    shouldThrow("crypto.getRandomValues(view)");
}

checkIntegerTypes();
checkNonIntegerTypes();

finishJSTest();
