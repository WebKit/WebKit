var refTestTimer = setTimeout(function() {}, 30000);

function takeScreenshot() {
	clearTimeout(refTestTimer);
}

function takeScreenshoDelayed(timeout) {
	setTimeout(function() {
		clearTimeout(refTestTimer);
	}, timeout);
}
