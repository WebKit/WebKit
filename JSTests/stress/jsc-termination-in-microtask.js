//@ skip if not $isWasmPlatform
//@ runDefault("--watchdog=10", "--watchdog-exception-ok")

async function asyncInfiniteLoop() {
  for (;;) {
    await undefined;
  }
}

// Queue up deferred work.
WebAssembly.instantiate(new Uint8Array());

// Wait for timeout in microtask queue.
asyncInfiniteLoop();
