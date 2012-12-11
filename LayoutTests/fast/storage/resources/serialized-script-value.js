function expectBufferValue(bytesPerElement, expectedValues, buffer) {
    expectedBufferValues = expectedValues;
    var arrayClass;
    if (bytesPerElement == 1)
        arrayClass = Uint8Array;
    else
        arrayClass = Uint16Array;
    bufferView = new arrayClass(buffer);
    shouldBe("bufferView.length", "expectedBufferValues.length");
    var success = (bufferView.length == expectedBufferValues.length);
    if (success) {
        for (var i = 0; i < expectedValues.length; i++) {
            if (expectedValues[i] != bufferView[i]) {
                testFailed("ArrayBufferViews differ at index " + i + ". Should be " + expectedValues[i] + ". Was " + bufferView[i]);
                success = false;
                break;
            }
        }
    }

    if (!success) {
        // output the full buffer for adding back into the test
        var output = [];
        for (i = 0; i < bufferView.length; i++) {
            var hexVal = bufferView[i].toString(16);
            if (hexVal.length < bytesPerElement * 2) {
                hexVal = "0000".slice(hexVal.length, bytesPerElement * 2) + hexVal;
            }
            output.push("0x" + hexVal);
        }
        debug("Actual buffer: [" + output.join(", ") + "]");
    }
}

function makeBuffer(bytesPerElement, serializedValues) {
    var arrayClass;
    if (bytesPerElement == 1)
        arrayClass = Uint8Array;
    else
        arrayClass = Uint16Array;

    var bufferView = new arrayClass(new ArrayBuffer(serializedValues.length * bytesPerElement));
    for (var i = 0; i < serializedValues.length; i++) {
        bufferView[i] = serializedValues[i];
    }
    return bufferView.buffer;
}


function _testSerialization(bytesPerElement, obj, values, oldFormat, serializeExceptionValue) {
    debug("");

    if (!serializeExceptionValue) {
        self.obj = obj;
        debug("Deserialize to " + JSON.stringify(obj) + ":");
        self.newObj = internals.deserializeBuffer(makeBuffer(bytesPerElement, values));
        shouldBe("JSON.stringify(newObj)", "JSON.stringify(obj)");

        if (oldFormat) {
            self.newObj = internals.deserializeBuffer(makeBuffer(bytesPerElement, oldFormat));
            shouldBe("JSON.stringify(newObj)", "JSON.stringify(obj)");
        }
    }

    debug("Serialize " + JSON.stringify(obj) + ":");
    try {
        var serialized = internals.serializeObject(obj);
        if (serializeExceptionValue) {
            testFailed("Should have thrown an exception of type ", serializeExceptionValue);
        }
    } catch(e) {
        if (!serializeExceptionValue) {
            testFailed("Threw exception " + e);
            return;
        } else {
            self.thrownException = e;
            self.expectedException = serializeExceptionValue;
            shouldBe("thrownException.code", "expectedException");
            return;
        }
    }
    expectBufferValue(bytesPerElement, values, serialized);
}
