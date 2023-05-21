//@ requireOptions("--useConcurrentJIT=0", "--jitPolicyScale=0")

function shouldThrow(func, errorMessage) {
  let errorThrown = false;
  try {
    func();
  } catch (error) {
    errorThrown = true;
    if (String(error) !== errorMessage)
      throw new Error(`Bad error: ${error}`);
  }
  if (!errorThrown)
    throw new Error(`Didn't throw!`);
}

let keyObj;
const StringProxy = new Proxy(String, {
  get(target, key) {
    keyObj = new String(key);
    return String;
  }
});

shouldThrow(() => { StringProxy.split(StringProxy); }, "TypeError: Cannot convert a symbol to a string");

if (!(keyObj instanceof String))
  throw new Error("Bad assertion!");
