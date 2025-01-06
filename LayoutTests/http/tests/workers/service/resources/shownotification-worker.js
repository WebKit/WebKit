
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

self.addEventListener('notificationclick', async function(event) {
    await messageClients("clicked|data:" + event.notification.data);
    event.notification.close();
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

    if (!self.Notification) {
        await messageClients("showFailed due to Notification not being exposed");
        return;
    }
    try {
         new Notification(title, {
            body: body,
            tag: tag,
            data: data
        });
        await messageClients("showFailed due to Notification created from constructor");
        return;
    } catch(error) {
        if (!(error instanceof TypeError)) {
            await messageClients("Notification constructor should throw a TypeError");
            return;
        }
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

async function tryShowInvalidData()
{
    let error = null;
    try {
        await registration.showNotification("Invalid notification", { data: function() { } });
    } catch (e) {
        error = e;
    }

    if (error)
        await messageClients("showFailed: threw " + error.name);
    else if (error0)
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
    if (messageName == "tryshowinvaliddata")
        await tryShowInvalidData();
    if (messageName == "getnotes")
        await getNotes(event.data);
});
