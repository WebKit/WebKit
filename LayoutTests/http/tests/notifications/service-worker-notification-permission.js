self.onmessage = (event) => {
  event.source.postMessage(Notification.permission);
};
