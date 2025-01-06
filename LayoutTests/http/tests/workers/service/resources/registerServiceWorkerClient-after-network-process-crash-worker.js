let messageClients = async function(msg) {
    const allClients = await clients.matchAll({
        includeUncontrolled: true
    });

    if (!allClients.length) {
        await new Promise(resolve => setTimeout(resolve, 50));
        return messageClients(msg);
    }

    for (const client of allClients)
        client.postMessage(msg);
}

self.addEventListener('message', async () => {
    const title = "my title";
    const body = "my body";
    const tag = "my tag";
    const data = "my data";

    try {
        await registration.showNotification(title, { body, tag, data });
    } catch(error) {
        await messageClients("showFailed");
        return;
    }

    await messageClients("started");
});

self.addEventListener('notificationclick', async function(event) {
    await messageClients("click");
});
