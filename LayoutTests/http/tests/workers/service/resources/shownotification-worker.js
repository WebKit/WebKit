
let messageClients = async function(msg) { 
    const allClients = await clients.matchAll({
        includeUncontrolled: true
    });

    for (const client of allClients)
        client.postMessage(msg);
}

self.addEventListener('notificationclick', async function(event) {
    await messageClients("clicked");
    event.notification.close();
});

self.addEventListener('notificationclose', async function(event) {
    await messageClients("closed");
});

self.addEventListener('message', async function(event) {
    // Reply if the show succeeded or failed.
    try {
        await registration.showNotification("This is a notification");
    } catch(error) {
        await messageClients("showFailed");
        return;
    }
    
    await messageClients("shown");
});
