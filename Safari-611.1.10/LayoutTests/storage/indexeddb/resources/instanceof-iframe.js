function test(callback) {
    isMainFrame = self == top;
    if (isMainFrame)
        evalAndLog("indexedDB.deleteDatabase('testDB')");
    else {
        shouldBe = function(a, b) {
            aVal = eval(a);
            bVal = eval(b);
            if (aVal != bVal)
                parent.testFailed(a + " is " + aVal + ", not " + bVal + ".");
            else 
                parent.testPassed(a + " equals to " + b  + ".");
        }
        shouldBeTrue = function(a) {
            shouldBe(a, "true");
        }
        shouldBeFalse = function(a) {
            shouldBe(a, "false");
        }
        evalAndLog = function(a) {
            parent.debug(a);
            return eval(a);
        }
    }

    openRequest = evalAndLog("indexedDB.open('testDB', 1)");
    openRequest.onupgradeneeded = () => { 
        request = evalAndLog("openRequest.result.createObjectStore('testObjectStore', {keyPath: 'id'})"); 
        request.onerror = unexpectedErrorCallback;
    }
    openRequest.onsuccess = () => {
        tx = evalAndLog("tx = openRequest.result.transaction('testObjectStore', 'readwrite')");
        tx.oncomplete = () => { callback(); }
        store = evalAndLog("store = tx.objectStore('testObjectStore')");

        if (isMainFrame)
            evalAndLog("store.put({id: 1, array:[1,2,3], arrayBuffer: new ArrayBuffer(3), set: new Set([1,2,3]), map: new Map([[1, 'one']]), object: { name: 'test' }})");

        request = evalAndLog("store.get(1)");
        request.onsuccess = (event) => {
            result = request.result;

            shouldBeTrue("result.array instanceof Array");
            shouldBeTrue("result.arrayBuffer instanceof ArrayBuffer");
            shouldBeTrue("result.set instanceof Set");
            shouldBeTrue("result.map instanceof Map");
            shouldBeTrue("result.object instanceof Object");
            expected = isMainFrame.toString();
            shouldBe("result.array instanceof window.top.Array", expected);
            shouldBe("result.arrayBuffer instanceof window.top.ArrayBuffer", expected);
            shouldBe("result.set instanceof window.top.Set", expected);
            shouldBe("result.map instanceof window.top.Map", expected);
            shouldBe("result.object instanceof window.top.Object", expected);
        }
    }
}

function callback() {
    iframe = document.getElementById("testIframe");
    iframe.srcdoc = `<!DOCTYPE html><html></` + `script><script type="text/javascript">${test.toString()} test(function() { parent.finishJSTest();});</` + `script></html>`;
}

test(callback);