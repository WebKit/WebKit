function done()
{
    finishSWTest();
}

navigator.serviceWorker.register("image-mime-type.php", { })
.then(function(r) {
	console.log("Registered! (unexpectedly)");
}, function(e) {
	console.log("Registration failed with error: " + e);
})
.catch(function(e) {
	console.log("Exception registering: " + e);
});

navigator.serviceWorker.register("", { })
.then(function(r) {
	console.log("Registered! (unexpectedly)");
}, function(e) {
	console.log("Registration failed with error: " + e);
})
.catch(function(e) {
	console.log("Exception registering: " + e);
});

navigator.serviceWorker.register("http://127.0.0.1:abc", { })
.then(function(r) {
	console.log("Registered! (unexpectedly)");
}, function(e) {
	console.log("Registration failed with error: " + e);
})
.catch(function(e) {
	console.log("Exception registering: " + e);
});

navigator.serviceWorker.register("somescheme://script.js", { })
.then(function(r) {
	console.log("Registered! (unexpectedly)");
}, function(e) {
	console.log("Registration failed with error: " + e);
})
.catch(function(e) {
	console.log("Exception registering: " + e);
});

navigator.serviceWorker.register("resources/%2fscript.js", { })
.then(function(r) {
	console.log("Registered! (unexpectedly)");
}, function(e) {
	console.log("Registration failed with error: " + e);
})
.catch(function(e) {
	console.log("Exception registering: " + e);
});

navigator.serviceWorker.register("resources/script.js", { scope: "somescheme://script.js" })
.then(function(r) {
	console.log("Registered! (unexpectedly)");
}, function(e) {
	console.log("Registration failed with error: " + e);
})
.catch(function(e) {
	console.log("Exception registering: " + e);
});

navigator.serviceWorker.register("resources/script.js", { scope: "%2fscript.js" })
.then(function(r) {
	console.log("Registered! (unexpectedly)");
}, function(e) {
	console.log("Registration failed with error: " + e);
})
.catch(function(e) {
	console.log("Exception registering: " + e);
});

navigator.serviceWorker.register("http://localhost:8000/workers/service/resources/empty-worker.js", { })
.then(function(r) {
	console.log("Registered! (unexpectedly)");
}, function(e) {
	console.log("Registration failed with error: " + e);
})
.catch(function(e) {
	console.log("Exception registering: " + e);
});

navigator.serviceWorker.register("resources/empty-worker.js", { scope: "http://localhost:8000/workers/service/" })
.then(function(r) {
	console.log("Registered! (unexpectedly)");
	done();
}, function(e) {
	console.log("Registration failed with error: " + e);
	done();
})
.catch(function(e) {
	console.log("Exception registering: " + e);
	done();
});
