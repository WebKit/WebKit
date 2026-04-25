//@ slow!
//@ runDefault("--useJIT=0", "--watchdog=6000", "--watchdog-exception-ok")
if (typeof fullGC === "undefined") globalThis.fullGC = () => Bun.gc(true);
if (typeof print === "undefined") globalThis.print = console.log;
async function initializeFileImporter() {
  for (let i = 1; i <= 50000; i++) {
    await new Promise(resolve => resolve());
    fullGC();
  }
}

async function main() {
  const addToLog = () => {};

  if (false) {
    addToLog(await foo(bar => ({ bar })));
  } else {
    await initializeFileImporter();
    addToLog();
  }
}

main().catch(e => print(e));
