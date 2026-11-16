async function doTest(event)
{
    let data = [];
    const currentClients = await self.clients.matchAll({includeUncontrolled: true});
    for (let client of currentClients) {
        data.push({id:client.id, visibilityState:client.visibilityState, focused:client.focused, frameType:client.frameType });
    }
    event.source.postMessage(data);
}
self.addEventListener("message", doTest);
