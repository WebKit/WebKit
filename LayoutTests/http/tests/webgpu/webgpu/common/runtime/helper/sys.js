/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/
function node() {
  const { existsSync } = require('fs');

  return {
    type: 'node',
    existsSync,
    args: process.argv.slice(2),
    cwd: () => process.cwd(),
    exit: (code) => process.exit(code)
  };
}










function deno() {
  function existsSync(path) {
    try {
      Deno.readFileSync(path);
      return true;
    } catch (err) {
      return false;
    }
  }

  return {
    type: 'deno',
    existsSync,
    args: Deno.args,
    cwd: Deno.cwd,
    exit: Deno.exit
  };
}

const sys = typeof globalThis.process !== 'undefined' ? node() : deno();

export default sys;
//# sourceMappingURL=sys.js.map