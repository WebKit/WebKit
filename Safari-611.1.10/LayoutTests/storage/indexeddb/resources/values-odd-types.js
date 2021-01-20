if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB odd value datatypes");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    objectStore = evalAndLog("db.createObjectStore('foo', {autoIncrement: true});");

    canvas = document.createElement('canvas');
    context = canvas.getContext('2d');
    imagedata = context.createImageData(1, 1);
    validTypes = [{ description: 'regexp',    value: new RegExp('\\\\') },
                  { description: 'date',      value: new Date(0) },
                  { description: 'object',    value: new Object() },
                  { description: 'imagedata', value: imagedata },
    ];
    nextToAdd = 0;
    addData();
}

function addData()
{
    debug("adding " + validTypes[nextToAdd].description + " value");
    result = evalAndLog("objectStore.add(validTypes[nextToAdd].value)");
    result.onsuccess = ++nextToAdd < validTypes.length ? addData : openACursor;
    result.onerror = unexpectedErrorCallback;
}

function openACursor()
{
    valueIndex = 0;
    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.value.toString()", "validTypes[valueIndex].value.toString()");
            if (valueIndex == 1) {
                shouldBeEqualToString("cursor.value.toUTCString()", "Thu, 01 Jan 1970 00:00:00 GMT");
            }
            if (valueIndex == 3) {
                shouldBe("cursor.value.width", "1");
            }
            evalAndLog("cursor.continue();");
            evalAndLog("valueIndex++;");
        }
        else {
            finishJSTest();
        }
    }
}
