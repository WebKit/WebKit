self.onmessage = function() {
    function test(method, args) {
        try {
            var request = self.indexedDB[method].call(self.indexedDB, args);
            return 'Successfully called self.indexedDB.' + method + '().<br>';
        } catch (exception) {
            return 'self.indexedDB.' + method + '() threw an exception: ' + exception.name + '<br>';
        }
    }
    self.postMessage({
        'result': [
            test('deleteDatabase', 'testDBName'),
            test('open', 'testDBName'),
        ]
    });
};
