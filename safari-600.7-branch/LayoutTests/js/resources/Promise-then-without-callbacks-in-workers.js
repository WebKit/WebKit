importScripts('../../resources/js-test-pre.js');

description('Test Promise.');

var global = this;

global.jsTestIsAsync = true;

new Promise(function(resolve) { resolve('hello'); })
  .then()
  .then(function(result) {
    global.result = result;
    shouldBeEqualToString('result', 'hello');
    finishJSTest();
  });


