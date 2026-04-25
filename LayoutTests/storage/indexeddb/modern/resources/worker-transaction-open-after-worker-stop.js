const errorHandler = function (event) {
  console.error(event.target.error);
}

console.log('Deleting database...');
var deleteRequest = indexedDB.deleteDatabase('test');
deleteRequest.onerror = deleteRequest.onblocked = deleteRequest.onsuccess = function () {
    console.log('Opening database...');
    var openRequest = indexedDB.open('test');
    openRequest.onerror = errorHandler;
    openRequest.onupgradeneeded = function () {
        var db = openRequest.result;
        db.createObjectStore('test', {keyPath: 'a'});
    }
    openRequest.onsuccess = function (event) {
        var db = event.target.result;
        var hasMessagedBack = false;

        // Queue up many transactions. 
        // We'll kill the worker from the main thread after the first transaction completes,
        // meaning there will be many more that would trigger the crash after the worker is gone.
        for (var i = 0; i < 1000; ++i) {
            var tx = db.transaction('test', 'readwrite');
            tx.onerror = errorHandler;
            tx.onabort = errorHandler;
            tx.oncomplete = function () {
                console.log('All done!');
                if (!hasMessagedBack) {
                    hasMessagedBack = true;
                    postMessage('First transaction completed');
                }
            };
        }
    };
};
