// This test makes sure that two different windows from two different domains who spam SW registrations get responses intertwined with each other
// as each registration should operate on its own task queue.

if (location.hostname != "127.0.0.1")
    alert("This test must be initiated from the hostname 127.0.0.1");

window.onmessage = function(evt)
{
	if (evt.data == "done") {
		alert("Error in popup window");
		finishSWTest();
	} else if (evt.data == "responded") {
		if (!window.popupTimestamp)
			window.popupTimestamp = new Date();
		
		responded();
	} else {
		alert("Unexpected event from popup window");
		finishSWTest();
	}
}

function responded()
{
	if (!window.thisTimestamp) {
		window.thisTimestamp = new Date();
	
		if (window.popupTimestamp)
			alert("Registrations in second window should not have responded before the first");
	
		return;
	}
	
	if (!window.popupTimestamp) 
		return;
		
	if (window.popupTimestamp < window.thisTimestamp)
		alert("Popup should not have popped up before this main window");
	else
		alert("Done");
	
	finishSWTest();
}

for (var i = 0; i < 1000; ++i) {
	navigator.serviceWorker.register("resources/empty-worker.js", { })
	.then(function(r) {
		console.log("Original window resolved successfully (unexpected)")
		done();
	}, function(e) {
		if (e+"" != "UnknownError: serviceWorker job scheduling is not yet implemented") {
			alert("Unexpected error received from server");
			finishSWTest();
		}
		
		responded();
	})
}

otherWindow = window.open("http://localhost:8000/workers/service/resources/registration-task-queue-scheduling-1-second-window.html", "other");
