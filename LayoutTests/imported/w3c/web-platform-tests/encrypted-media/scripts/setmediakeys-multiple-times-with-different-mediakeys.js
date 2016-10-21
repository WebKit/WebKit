function runTest(config, qualifier) {
    var testname = testnamePrefix( qualifier, config.keysystem )
                         + ', setmediakeys multiple times with different mediakeys';

    var configuration = getSimpleConfigurationForContent( config.content );

    async_test (function (test) {
        var _video = config.video,
            _access,
            _mediaKeys1,
            _mediaKeys2,
            fail;

        // Test MediaKeys assignment.
        assert_equals(_video.mediaKeys, null);
        assert_equals(typeof _video.setMediaKeys, 'function');

        function onFailure(error) {
            forceTestFailureFromPromise(test, error);
        }

        navigator.requestMediaKeySystemAccess(config.keysystem, [configuration]).then(function(access) {
            _access = access;
            return _access.createMediaKeys();
        }).then(test.step_func(function(result) {
            _mediaKeys1 = result;
            assert_not_equals(_mediaKeys1, null);
            // Create a second mediaKeys.
            return _access.createMediaKeys();
        })).then(test.step_func(function(result) {
            _mediaKeys2 = result;
            assert_not_equals(_mediaKeys2, null);
            // Set _mediaKeys1 on video.
            return _video.setMediaKeys(_mediaKeys1);
        })).then(test.step_func(function() {
            assert_equals(_video.mediaKeys, _mediaKeys1);
            // Set _mediaKeys2 on video (switching MediaKeys).
            return _video.setMediaKeys(_mediaKeys2);
        })).then(test.step_func(function() {
            assert_equals(_video.mediaKeys, _mediaKeys2);
            // Clear mediaKeys from video.
            return _video.setMediaKeys(null);
        })).then(test.step_func(function() {
            assert_equals(_video.mediaKeys, null);
            // Set _mediaKeys1 on video again.
            return _video.setMediaKeys(_mediaKeys1);
        })).then(test.step_func(function() {
            assert_equals(_video.mediaKeys, _mediaKeys1);
            return testmediasource(config);
        })).then(function(source) {
            // Set src attribute on Video Element
            _video.src = URL.createObjectURL(source);
            // Set mediaKeys2 on video element (switching MediaKeys) need not
            // fail after src attribute is set.
            return _video.setMediaKeys(_mediaKeys2);
        })).then(test.step_func(function() {
            // Switching setMediaKeys after setting src attribute on video element
            // is not required to fail.
            assert_equals(_video2.mediaKeys, _mediaKeys2);
            fail = false;
            return Promise.resolve();
        }, test.step_func(function(error) {
            fail = true;
            assert_equals(_video.mediaKeys, _mediaKeys1);
            assert_in_array(error.name, ['InvalidStateError','NotSupportedError']);
            assert_not_equals(error.message, '');
            // Return something so the promise resolves properly.
            return Promise.resolve();
        })).then(function() {
            // Set null mediaKeys on video (clearing MediaKeys) not
            // supported after src attribute is set.
            return _video.setMediaKeys(null);
        }).then(test.step_func(function() {
            assert_unreached('Clearing mediaKeys after setting src should have failed.');
        }), test.step_func(function(error) {
            if(fail) {
                assert_equals(_video.mediaKeys, _mediaKeys1);
            } else {
                assert_equals(_video.mediaKeys, _mediaKeys2);
            }
            assert_is_array(error.name, ['InvalidStateError','ReferenceError']);
            assert_not_equals(error.message, '');
            test.done();
        })).catch(onFailure);
    }, testname);
}