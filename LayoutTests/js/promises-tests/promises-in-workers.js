'use strict';

if (self.importScripts) {
  self.importScripts('../../resources/testharness.js');
}

var a = [];

// Changing from 2 to 1 makes the test pass.
for (let i = 0; i < 2; i++) {
  promise_test(t => {

    for (let j = 0; j < 10000; j++)
      a.push(new Promise(function() {}));

    return new Promise(resolve => step_timeout(resolve, 500));

  }, 'Test');
}

done();
