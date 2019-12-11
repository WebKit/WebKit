if (self.importScripts)
  self.importScripts('../../../../resources/testharness.js');

test(function() {
    assert_throws(new TypeError(), function() { new FileReaderSync().readAsBinaryString(null); });
    assert_throws(new TypeError(), function() { new FileReaderSync().readAsArrayBuffer(null); });
    assert_throws(new TypeError(), function() { new FileReaderSync().readAsText(null); });
    assert_throws(new TypeError(), function() { new FileReaderSync().readAsDataURL(null); });
}, "Trying to read a null parameter");


test(function() {
    assert_throws(new TypeError(), function() { new FileReaderSync().readAsBinaryString(undefined); });
    assert_throws(new TypeError(), function() { new FileReaderSync().readAsArrayBuffer(undefined); });
    assert_throws(new TypeError(), function() { new FileReaderSync().readAsText(undefined); });
    assert_throws(new TypeError(), function() { new FileReaderSync().readAsDataURL(undefined); });
}, "Trying to read an undefined parameter");

done();
