function test() {
var passed = false;
function asyncTestPassed() {
    passed = true;
}

class P extends Promise {}
var p1 = new P(function(resolve, reject) { resolve("foo"); });
var p2 = new P(function(resolve, reject) { reject("quux"); });
var score = +(p1 instanceof P);

function thenFn(result)  { score += (result === "foo");  check(); }
function catchFn(result) { score += (result === "quux"); check(); }
function shouldNotRun(result)  { score = -Infinity;   }

p1.then(thenFn, shouldNotRun);
p2.then(shouldNotRun, catchFn);
p1.catch(shouldNotRun);
p2.catch(catchFn);

p1.then(function() {
  // P.prototype.then() should return a new P
  score += p1.then() instanceof P && p1.then() !== p1;
  check();
});

function check() {
  if (score === 5) asyncTestPassed();
}
      
drainMicrotasks();
return passed;
}

if (!test())
    throw new Error("Test failed");

