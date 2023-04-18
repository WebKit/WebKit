if (navigator.storage) {
	navigator.storage.estimate().then((estimate) => {
		self.postMessage(estimate);
	}).catch((error) => {
		self.postMessage('fail');
	}); 
} else {
	self.postMessage('navigator.storage is not available');
}