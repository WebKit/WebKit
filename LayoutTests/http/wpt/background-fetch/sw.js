let waitForClickPort, waitForAbortPort;
onmessage = (event) => {
  if (event.data.type === "waitForClick")
    waitForClickPort = event.data.port;
  if (event.data.type === "waitForAbort")
    waitForAbortPort = event.data.port;
}

onbackgroundfetchclick = (event) => {
  if (waitForClickPort)
    waitForClickPort.postMessage(event.registration.id);
}

onbackgroundfetchabort= (event) => {
  if (waitForAbortPort)
    waitForAbortPort.postMessage(event.registration.id);
}
