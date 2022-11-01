onconnect = (e) => {
  const port = e.ports[0];
  const importCount = Math.trunc(Math.random() * 5);

  for (let i = 0; i < importCount; i++) {
    importScripts(`imported_script_delay.py?cache_bust=${i}`);
  }

  port.postMessage("worker_finished");
};
