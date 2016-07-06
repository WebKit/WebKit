// This will prevent work to update tests to use unprefixed objects in
// the future.  It wll also make it possible to bisect builds using
// a WebKit that only supports prefixed objects.

if (window.hasOwnProperty('webkitAudioContext') && !window.hasOwnProperty('AudioContext')) {
    window.AudioContext = webkitAudioContext;
    window.OfflineAudioContext = webkitOfflineAudioContext;
}
