onmessage = (e) => {
    if (e.data == "check-initial-permission" || e.data == "check-permission-granted") {
        self.postMessage(Notification.permission);
        return;
    }
    if (e.data == "show-notification") {
        notification = new Notification("foo");
        timeoutHandle = setTimeout(() => {
            self.postMessage("timeout");
        }, 10000);
        notification.onshow = (e) => {
            clearTimeout(timeoutHandle);
            self.postMessage("shown");
        };
        notification.onerror = (e) => {
            clearTimeout(timeoutHandle);
            self.postMessage("error");
        };
    }
};
