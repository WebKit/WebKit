(function() {
    if (window.__xGoogleSignInButtonFix)
        return;
    window.__xGoogleSignInButtonFix = true;
    var style = document.createElement('style');
    style.textContent = '.jf-gsi-hit > div > div:first-child:empty:not([role]) { display: none !important }';
    (document.head || document.documentElement).appendChild(style);
})();
