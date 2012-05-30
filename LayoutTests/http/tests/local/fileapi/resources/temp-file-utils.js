function createTempFile(fileName, fileData)
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "http://127.0.0.1:8000/resources/write-temp-file.php?filename=" + fileName + "&data=" + fileData, false);
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
    xhr.open("POST", "http://127.0.0.1:8000/resources/touch-temp-file.php?filename=" + fileName, false);
    xhr.send();
}

function removeTempFile(fileName)
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "http://127.0.0.1:8000/resources/reset-temp-file.php?filename=" + fileName, false);
    xhr.send();
}
