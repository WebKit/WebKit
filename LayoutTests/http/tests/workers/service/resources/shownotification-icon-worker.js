self.addEventListener("message", async event => {
    try {
        await registration.showNotification("Notification with icon", { icon: "/resources/square100.png" });
        for (const notification of await registration.getNotifications())
            notification.close();
        event.source.postMessage("shown");
    } catch (error) {
        event.source.postMessage("showFailed: " + error);
    }
});
