//@ requireOptions("--useShadowRealm=1")

let sr = new ShadowRealm;

let install = sr.evaluate(`
(function(name, fn) {
  globalThis[name] = fn;
})
`);

let log = function(...args) {
  let string = args.join(" ");
  print(string);
  return string;
};
install("log", log);

// Test that the GlobalObject prototype is not immutable, 
let MAX_ITER = 10000;
sr.evaluate(`
  var i = 1;
  function test() {
    globalThis.__proto__ = { x: i++ };
  }
  for (let i = 0; i < ${MAX_ITER}; ++i) {
    try {
      test();
      if (globalThis.x !== i + 1)
        throw new Error(\`Prototype not written successfully (Expected globalThis.x === \${i + 1}, but found \${globalThis.x})\`);
    } catch (e) {
      log(\`\${e}\`);
      throw e;
    }
  }
`);

if (sr.evaluate(`globalThis.x`) !== MAX_ITER)
  throw new Error("Prototype invalid in separate eval");

