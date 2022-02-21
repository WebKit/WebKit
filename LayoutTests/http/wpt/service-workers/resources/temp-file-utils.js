function createTempFile(fileName, fileData)
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/WebKit/service-workers/resources/write-temp-file.py?filename=" + fileName + "&data=" + fileData, false);
    xhr.send();
    if (xhr.status != 200) {
        testFailed("Unexpected response status received: " + xhr.status);
        return;
    }

    var values = xhr.responseText.split('\n');
    if (xhr.responseText.indexOf("FAIL") == 0) {
        testFailed("Unexpected response text received: " + xhr.responseText);
        return;
    }

    return xhr.responseText;
}

function touchTempFile(fileName)
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/WebKit/service-workers/resources/touch-temp-file.py?filename=" + fileName, false);
    xhr.send();
}

function removeTempFile(fileName)
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/WebKit/service-workers/resources/reset-temp-file.py?filename=" + fileName, false);
    xhr.send();
}
