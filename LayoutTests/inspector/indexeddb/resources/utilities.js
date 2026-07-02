function deleteDatabaseNames(names) {
    for (let name of names)
        window.indexedDB.deleteDatabase(name);
}

function createEmptyDatabase(name, version=1) {
    let request = window.indexedDB.open(name, version);
    request.addEventListener('success', function(event) {
        console.log(`Created Database '${name}'`);
        let db = event.target.result;
        db.close();
        TestPage.dispatchEventToFrontend("DatabaseCreated");
    });
}

function createDatabaseWithStores(name, version) {
    let request = window.indexedDB.open(name, version);
    request.addEventListener("success", function(event) {
        console.log(`Created Database '${name}'`);
        TestPage.dispatchEventToFrontend("DatabaseCreated");
    });

    request.addEventListener("upgradeneeded", function(event) {
        let db = event.target.result;

        // Reviewers => {name, email}
        let reviewerObjectStore = db.createObjectStore("Reviewers", {autoIncrement: true});
        reviewerObjectStore.createIndex("Name Index", "name", {unique: false});
        reviewerObjectStore.createIndex("Email Index", "email", {unique: true});

        // Stats => {name, count}
        let statsObjectStore = db.createObjectStore("Stats", {keyPath: "name"});
        statsObjectStore.createIndex("Directory Name Index", "name", {unique: true});
        statsObjectStore.createIndex("File Count Index", "count", {unique: false});

        // Empty => any value
        let lastObjectStore = db.createObjectStore("Empty");

        // Populate once stores are created.
        lastObjectStore.transaction.oncomplete = (event) => {
            let reviewerObjectStore = db.transaction("Reviewers", "readwrite").objectStore("Reviewers");
            reviewerObjectStore.add({name: "Thirsty Hamster", email: "hamster@webkit.org"});
            reviewerObjectStore.add({name: "Jamming Peacock", email: "peacock@webkit.org"});
            reviewerObjectStore.add({name: "Bustling Badger", email: "badger@webkit.org"});
            reviewerObjectStore.add({name: "Monstrous Beaver", email: "beaver@webkit.org"});

            let statsObjectStore = db.transaction("Stats", "readwrite").objectStore("Stats");
            statsObjectStore.add({name: "Images", count: 250, extensions: ["png", "svg"]});
            statsObjectStore.add({name: "Models", count: 109, extensions: ["js"]});
            statsObjectStore.add({name: "Views", count: 436, extensions: ["css", "js"]});
            statsObjectStore.add({name: "Controllers", count: 41, extensions: ["css", "js"]});
        };
    });
};
