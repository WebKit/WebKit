function test() {
var passed = false;
function asyncTestPassed() {
    passed = true;
}

class P extends Promise {}
var fulfills = P.race([
  new Promise(function(resolve)   { setTimeout(resolve,200,"foo"); }),
  new Promise(function(_, reject) { setTimeout(reject, 300,"bar"); }),
]);
var rejects = P.race([
  new Promise(function(_, reject) { setTimeout(reject, 200,"baz"); }),
  new Promise(function(resolve)   { setTimeout(resolve,300,"qux"); }),
]);
var score = +(fulfills instanceof P);
fulfills.then(function(result) { score += (result === "foo"); check(); });
rejects.catch(function(result) { score += (result === "baz"); check(); });

function check() {
  if (score === 3) asyncTestPassed();
}
      
drainMicrotasks();
return passed;
}

if (!test())
    throw new Error("Test failed");

