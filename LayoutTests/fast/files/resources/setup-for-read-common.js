function log(message)
{
    document.getElementById('console').appendChild(document.createTextNode(message + "\n"));
}

function onInputFileChange(testFileInfoList)
{
    var files = document.getElementById("file").files;
    var testFiles = { };
    for (var i = 0; i < files.length; i++)
        testFiles[testFileInfoList[i]['name']] = files[i];

    startTest(testFiles);
}

function runTests(testFileInfoList)
{
    var pathsOnly = testFileInfoList.map(function(fileSpec) { return fileSpec['path']; });
    eventSender.beginDragWithFiles(pathsOnly);
    eventSender.mouseMoveTo(10, 10);
    eventSender.mouseUp();
}
