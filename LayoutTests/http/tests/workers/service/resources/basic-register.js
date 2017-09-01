function done()
{
    finishSWTest();
}

navigator.serviceWorker.register("resources/empty-worker.js", { })
.then(function(r) {
	console.log("Registered!");
}, function(e) {
	console.log("Registration failed with error: " + e);
})
.catch(function(e) {
	console.log("Exception registering: " + e);
});

navigator.serviceWorker.register("resources/empty-worker-doesnt-exist.js", { })
.then(function(r) {
	console.log("Registered!");
	done();
}, function(e) {
	console.log("Registration failed with error: " + e);
	done();
})
.catch(function(e) {
	console.log("Exception registering: " + e);
	done();
});
