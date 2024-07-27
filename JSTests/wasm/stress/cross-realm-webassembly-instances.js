// Copied from LayoutTests/imported/w3c/web-platform-tests/wasm/jsapi/proto-from-ctor-realm.html for jsc-only testing.

const constructorRealm = createGlobalObject();
const newTargetRealm = createGlobalObject();
const otherRealm = createGlobalObject();


const randomModuleBinary = new Uint8Array([ 0,97,115,109,1,0,0,0,1,136,128,128,128,0,2,96,0,0,96,0,1,124,3,131,128,128,128,0,2,0,1,7,138,128,128,128,0,1,6,114,117,110,102,54,52,0,1,10,165,128,128,128,0,2,130,128,128,128,0,0,11,152,128,128,128,0,0,3,124,68,0,0,0,0,0,0,0,0,65,0,14,0,1,16,0,11,12,0,0,11 ]);
const interfaces = [
  ["Module", randomModuleBinary],
  ["Instance", new WebAssembly.Module(randomModuleBinary)],
  ["Memory", {initial: 0}],
  ["Table", {element: "anyfunc", initial: 0}],
  ["Global", {value: "i32"}],

  // See step 2 of https://tc39.es/ecma262/#sec-nativeerror
  ["CompileError"],
  ["LinkError"],
  ["RuntimeError"],
];

const primitives = [
  undefined,
  null,
  false,
  true,
  0,
  -1,
  "",
  "str",
  Symbol(),
];

const getNewTargets = function* (realm) {
  for (const primitive of primitives) {
    const newTarget = new realm.Function();
    newTarget.prototype = primitive;
    yield [newTarget, "cross-realm NewTarget with `" + typeof(primitive) + "` prototype"];
  }

  // GetFunctionRealm (https://tc39.es/ecma262/#sec-getfunctionrealm) coverage:
  const bindOther = otherRealm.Function.prototype.bind;
  const ProxyOther = otherRealm.Proxy;

  const bound = new realm.Function();
  bound.prototype = undefined;
  yield [bindOther.call(bound), "bound cross-realm NewTarget with `undefined` prototype"];

  const boundBound = new realm.Function();
  boundBound.prototype = null;
  yield [bindOther.call(bindOther.call(boundBound)), "bound bound cross-realm NewTarget with `null` prototype"];

  const boundProxy = new realm.Function();
  boundProxy.prototype = false;
  yield [bindOther.call(new ProxyOther(boundProxy, {})), "bound Proxy of cross-realm NewTarget with `false` prototype"];

  const proxy = new realm.Function();
  proxy.prototype = true;
  yield [new ProxyOther(proxy, {}), "Proxy of cross-realm NewTarget with `true` prototype"];

  const proxyProxy = new realm.Function();
  proxyProxy.prototype = -0;
  yield [new ProxyOther(new ProxyOther(proxyProxy, {}), {}), "Proxy of Proxy of cross-realm NewTarget with `-0` prototype"];

  const proxyBound = new realm.Function();
  proxyBound.prototype = NaN;
  yield [new ProxyOther(bindOther.call(proxyBound), {}), "Proxy of bound cross-realm NewTarget with `NaN` prototype"];
};

let currentTestName;
function test(func, name) {
  currentTestName = name;
  func();
}

function assert_equal(a, b) {
  if (a === b)
    return;
  throw new Error(currentTestName + " failed");
}

for (const [interfaceName, constructorArg] of interfaces) {
  for (const [newTarget, testDescription] of getNewTargets(newTargetRealm)) {
    test(() => {
      const Constructor = constructorRealm.WebAssembly[interfaceName];
      const object = Reflect.construct(Constructor, [constructorArg], newTarget);

      const NewTargetConstructor = newTargetRealm.WebAssembly[interfaceName];
      assert_equal(object instanceof NewTargetConstructor, true);
      assert_equal(Object.getPrototypeOf(object), NewTargetConstructor.prototype);
    }, `WebAssembly.${interfaceName}: ${testDescription}`);
  }
}