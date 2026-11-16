if (self.navigator && navigator.storage && navigator.storage.estimate) {
    for (let i = 0; i < 5; ++i)
        navigator.storage.estimate();

    postMessage('started-estimate');

    close();
} else {
    postMessage('no-storage-estimate');
}
