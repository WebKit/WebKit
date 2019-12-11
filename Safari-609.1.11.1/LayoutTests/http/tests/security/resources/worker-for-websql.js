self.onmessage = function() {
	try {
		var db = self.openDatabase('testdb', '1.0', 'Testing database', 512 * 1024);
		self.postMessage(null);
	} catch (exception) {
		self.postMessage(exception.name);
	}
}
