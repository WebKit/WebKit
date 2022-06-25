function test() {
var passed = false;
function asyncTestPassed() {
    passed = true;
}

var fulfills = Promise.all([
  new Promise(function(resolve)   { setTimeout(resolve,200,"foo"); }),
  new Promise(function(resolve)   { setTimeout(resolve,100,"bar"); }),
]);
var rejects = Promise.all([
  new Promise(function(_, reject) { setTimeout(reject, 200,"baz"); }),
  new Promise(function(_, reject) { setTimeout(reject, 100,"qux"); }),
]);
var score = 0;
fulfills.then(function(result) { score += (result + "" === "foo,bar"); check(); });
rejects.catch(function(result) { score += (result === "qux"); check(); });

function check() {
  if (score === 2) asyncTestPassed();
}
      
drainMicrotasks();
return passed;
}

if (!test())
    throw new Error("Test failed");

