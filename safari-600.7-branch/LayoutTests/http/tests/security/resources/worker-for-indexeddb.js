self.onmessage = function() {
    function test(method, args) {
        try {
            var request = self.webkitIndexedDB[method].call(self.webkitIndexedDB, args);
            return 'Successfully called self.webkitIndexedDB.' + method + '().<br>';
        } catch (exception) {
            return 'self.webkitIndexedDB.' + method + '() threw an exception: ' + exception.name + '<br>';
        }
    }
    self.postMessage({
        'result': [
            test('deleteDatabase', 'testDBName'),
            test('open', 'testDBName'),
            test('webkitGetDatabaseNames')
        ]
    });
};
