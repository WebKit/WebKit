addEventListener('message', async event => {
    const clients = await self.clients.matchAll({ includeUncontrolled : true });
    clients.forEach(client => client.postMessage(event.data));
});
