onfetch = e => {
    e.respondWith(new Promise((resolve, reject) => {
        e.preloadResponse.then(response => { 
            if (response) {
                resolve(response);
                return;
            }
            fetch(e.request).then(resolve, reject);
        }, () => {
            fetch(e.request).then(resolve, reject);
        });
    }));
}
