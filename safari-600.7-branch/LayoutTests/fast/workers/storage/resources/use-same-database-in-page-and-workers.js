var db = openDatabase("UseSameDatabaseInPageAndWorkers", "", "", 1);
for (var i = 0; i < 100; i++) {
    db.transaction(function(tx) {
        tx.executeSql("INSERT INTO Test VALUES (?)", [i], null,
                      function(tx, error) { postMessage(error.message); });
      }, function(error) { postMessage(error.message); },
      function() { postMessage("transactionSuccessful"); });
}
