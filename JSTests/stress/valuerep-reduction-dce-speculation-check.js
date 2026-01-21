//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0")

function fn(a1) {
  try {
    throw 1;
  } catch {
    +a1;
    eval('1');
  }
}

let num_to_primitive = 0;
const obj = {};
obj[Symbol.toPrimitive] = a => num_to_primitive++;

fn(1.1);
fn(1);
fn(obj);

if (num_to_primitive != 1)
  throw Error("Did not call toPrimitive");
