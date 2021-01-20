const log = txt => document.writeln(`${txt}<br/>`);
log("Deleting db");
indexedDB.deleteDatabase("db").onsuccess = ()=> {
    log("db deleted");
    let req = indexedDB.open("db", 1);
    req.onupgradeneeded = e => {
        log ("Creating db");
        let db = req.transaction.db;
        let schools = db.createObjectStore('schools', { keyPath: 'id', autoIncrement: true });
        schools.createIndex ('city', 'city');
        schools.add({name: 'Stockholm University', city: 'Stockholm'});
        schools.add({name: 'Uppsala University', city: 'Uppsala'});
        schools.add({name: 'Chalmers', city: 'Gothenburg'});
        let students = db.createObjectStore('students', { keyPath: 'id', autoIncrement: true });
        students.createIndex ('name', 'name');
        students.add({name: 'Adam Anderson'});
        students.add({name: 'Bertil Bengtson'});
    }
    req.onsuccess = ()=> {
        let db = req.result;
        let tx = db.transaction(['schools', 'students'], 'readonly');
        dump(tx.objectStore('schools'), ()=> {
            dump(tx.objectStore('schools').index('city'), ()=> {
                dump(tx.objectStore('students'), ()=>{
                    dump(tx.objectStore('students').index('name'), ()=>{
                        log("Done.");
						finishJSTest();
                    })
                });
            });
        });
    }
}

function dump (idx, done) {
    log (idx instanceof IDBObjectStore ?
        `Enumerating ObjectStore '${idx.name}' by primary key` :
        `Enumerating index '${idx.name}' on '${idx.objectStore.name}'`);
    idx.openCursor().onsuccess = e => {
        let cursor = e.target.result; 
        if (cursor) {
            log(`key: ${cursor.key}, primaryKey: ${cursor.primaryKey}, value: ${JSON.stringify(cursor.value)}`);
            cursor.continue();
        } else {
            done();
        }
    }
}
