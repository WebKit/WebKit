
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

async function tryShow(message)
{
    var title, body, tag;
    var components = message.split('|');

    if (components.length == 1) {
        title = "This is a notification";        
    } else {
        title = components[1];
        body = components[2];
        tag = components[3];
    }
    
    try {
        await registration.showNotification(title, {
            body: body,
            tag: tag
        });
    } catch(error) {
        await messageClients("showFailed");
        return;
    }
    
    await messageClients("shown");
}

var seenNotes = new Set();

async function getNotes(message)
{
    var tag = undefined;
    var components = message.split('|');
    if (components.length == 2)
        tag = components[1];

    var notifications = await registration.getNotifications({ tag: tag });
    var reply = "gotnotes|There are " + notifications.length + " notifications|";

    for (notification of notifications) {
        if (seenNotes.has(notification))
            messageClients("Saw the same notifcation twice through getNotifications(), this should not happen");
        seenNotes.add(notification)
        
        reply += "Title: " + notification.title + "|";
        reply += "Body: " + notification.body + "|";
        reply += "Tag: " + notification.tag + "|";
    }
    await messageClients(reply);
}

self.addEventListener('message', async function(event) {
    var messageName = event.data.split('|')[0];
    if (messageName == "tryshow")
        await tryShow(event.data);
    if (messageName == "getnotes")
        await getNotes(event.data);
});
