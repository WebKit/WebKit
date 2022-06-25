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

        var tx = db.transaction('test', 'readwrite');
        tx.onerror = errorHandler;
        tx.onabort = errorHandler;
        tx.oncomplete = function () {
            console.log('All done!');
			postMessage('All done!');
        };

        var getAllRequest = tx.objectStore('test').getAll();
        getAllRequest.onerror = errorHandler;
        getAllRequest.onsuccess = function () {
            console.log('Success!');
        };
    };
};
