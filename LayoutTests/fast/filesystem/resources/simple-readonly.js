if (this.importScripts) {
    importScripts('../resources/fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('../resources/fs-test-util.js');
}

description("FileSystem API readonly attributes test.");

var fileSystem;
var fileEntry;
var fileMetadata;

function setReadonlyProperty(property, value)
{
    oldValue = eval(property);
    debug("trying to set readonly property " + property);
    evalAndLog(property + " = " + value);
    newValue = eval(property);
    if (oldValue == newValue) {
        testPassed(property + " is still " + oldValue);
    } else {
        testFailed(property + " value was changed");
    }
}

function errorCallback(err) {
  testFailed(err);
  finishJSTest();
}

function getFileCallback(entry) {
  fileEntry = entry;
  setReadonlyProperty("fileEntry.isFile", "false");
  setReadonlyProperty("fileEntry.isDirectory", "true");
  setReadonlyProperty("fileEntry.name", "'bar'");
  setReadonlyProperty("fileEntry.fullPath", "'bar'");
  setReadonlyProperty("fileEntry.filesystem", "null");
  finishJSTest();
}

function successCallback(fs) {
  fileSystem = fs;
  setReadonlyProperty("fileSystem.name", "'bar'");
  root = evalAndLog("root = fileSystem.root");
  evalAndLog("root.getFile('foo', {create:true}, getFileCallback, errorCallback)");
}

var jsTestIsAsync = true;
evalAndLog("webkitRequestFileSystem(TEMPORARY, 100, successCallback, errorCallback)");
