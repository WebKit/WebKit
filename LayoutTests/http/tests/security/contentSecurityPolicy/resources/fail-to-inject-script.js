var s = document.createElement('script');
s.onerror = function() {
    done();
};
s.onload = function() {
    assert_unreached('Script loaded.');
};
document.body.appendChild(s);
s.innerText = 'assert_unreached("Script should not run.");'
