//@ runDefault("--watchdog=100", "--watchdog-exception-ok")
(async function () {
  let bytes = readFile('./resources/large.wasm', 'binary');
  let x = await WebAssembly.instantiate(bytes, {
    wasi_snapshot_preview1: {
      fd_write() { return 0; },
      fd_close() {},
      fd_seek() {},
      proc_exit() {},
    },
  });
  x.instance.exports._start();
})().catch(e => debug(e));
