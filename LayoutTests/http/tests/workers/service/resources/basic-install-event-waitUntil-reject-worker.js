self.addEventListener("install", (event) => {
    event.waitUntil(new Promise((resolve, reject) => {
        setTimeout(() => {
            reject();
        }, 50);
    }));
});

