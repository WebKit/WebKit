importScripts('../../../resources/js-test-pre.js');

var global = this;
global.jsTestIsAsync = true;

description("Tests XMLHttpRequest.setRequestHeader() in workers");

var xhr = new XMLHttpRequest;
xhr.open("GET", "empty-worker.js", false);
xhr.setRequestHeader("Accept", "*/*");
xhr.send(null);

finishJSTest();

