let waitForClickPort, waitForAbortPort, waitForSuccessPort;
let expectedRecord;
onmessage = (event) => {
  if (event.data.type === "waitForClick")
    waitForClickPort = event.data.port;
  if (event.data.type === "waitForAbort")
    waitForAbortPort = event.data.port;
  if (event.data.type === "waitForSuccess") {
    waitForSuccessPort = event.data.port;
    expectedRecord = event.data.record;
  }
}

onbackgroundfetchclick = (event) => {
  if (waitForClickPort)
    waitForClickPort.postMessage(event.registration.id);
}

onbackgroundfetchabort= (event) => {
  if (waitForAbortPort)
    waitForAbortPort.postMessage(event.registration.id);
}

async function sendResponseText(event)
{
  if (!waitForSuccessPort)
    return;
  try {
    const record = await event.registration.match(expectedRecord);
    const response = await record.responseReady;
    waitForSuccessPort.postMessage(await response.text());
  } catch(e) {
    waitForSuccessPort.postMessage("" + e);
  }
}

onbackgroundfetchsuccess = async (event) => {
  event.waitUntil(sendResponseText(event));
}
