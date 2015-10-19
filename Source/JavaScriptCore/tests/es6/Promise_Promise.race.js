function test() {
var passed = false;
function asyncTestPassed() {
    passed = true;
}

var fulfills = Promise.race([
  new Promise(function(resolve)   { setTimeout(resolve,200,"foo"); }),
  new Promise(function(_, reject) { setTimeout(reject, 300,"bar"); }),
]);
var rejects = Promise.race([
  new Promise(function(_, reject) { setTimeout(reject, 200,"baz"); }),
  new Promise(function(resolve)   { setTimeout(resolve,300,"qux"); }),
]);
var score = 0;
fulfills.then(function(result) { score += (result === "foo"); check(); });
rejects.catch(function(result) { score += (result === "baz"); check(); });

function check() {
  if (score === 2) asyncTestPassed();
}
      
drainMicrotasks();
return passed;
}

if (!test())
    throw new Error("Test failed");

