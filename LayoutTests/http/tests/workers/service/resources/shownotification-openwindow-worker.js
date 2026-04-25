
let messageClients = async function(msg) { 
    const allClients = await clients.matchAll({
        includeUncontrolled: true
    });

    for (const client of allClients)
        client.postMessage(msg);
}

// Try to open a new window now.
// It should fail due to lack of user gesture
clients.openWindow('http://127.0.0.1:8000/workers/service/resources/openwindow-client.html').then(async function(client) {
    await messageClients("Opening a window client without a user gesture SUCCEEDED. It should've failed.");
}).catch(async function(error) {
    await messageClients("gotUserGestureFail");
});

self.addEventListener('notificationclick', async function(event) {
    await messageClients("clicked|data:" + event.notification.data);
    event.notification.close();

    // Should fail because about:blank is not an allowable URL
    clients.openWindow('about:blank').then(async function(client) {
        await messageClients("Opening a window client to about:blank SUCCEEDED. It should've failed.");
    }).catch(async function(error) {
        await messageClients("gotAboutBlankFail");
    });

    // This successfully loads the openwindow-client.html resource, but the origin is different, therefore the serviceworker
    // should NOT get a WindowClient
    clients.openWindow('http://localhost:8000/workers/service/resources/openwindow-client.html').then(async function(client) {
        if (client == null)
            await messageClients("gotSuccessfulNullClient")
        else
            await messageClients("Got a WindowClient when we were *not* expecting to");
    }).catch(async function(error) {
        await messageClients("Expected a resolved promise with a null window client. Promise rejected instead");
    });

    clients.openWindow('http://127.0.0.1:8000/workers/service/resources/openwindow-client.html').then(function(client) {
        client.postMessage("echo");
    });
});

self.addEventListener('notificationclose', async function(event) {
    await messageClients("closed");
});

async function tryShow(message)
{
    var command, title, body, tag, data;
    var components = message.split('|');

    if (components.length == 1) {
        title = "This is a notification";        
    } else if (components.length == 4) {
        [command, title, body, tag] = components;
    } else if (components.length == 5) {
        [command, title, body, tag, data] = components;
    }
    
    try {
        await registration.showNotification(title, {
            body: body,
            tag: tag,
            data: data
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
        reply += "Data: " + notification.data + "|";
    }
    await messageClients(reply);
}

self.addEventListener('message', async function(event) {
    var messageName = event.data.split('|')[0];
    if (messageName == "tryshow")
        await tryShow(event.data);
    if (messageName == "getnotes")
        await getNotes(event.data);
    if (messageName == "echo back")
        await messageClients("gotNewClient");
});
