<html>
    <head>
        <base href="<?php echo 'http://' . $_SERVER['HTTP_HOST'] . '/uri/resources/'; ?>">
        <style>
            @import 'css-href.css';
        </style>
    </head>
    <body>
        <p>Test for <a href="http://bugs.webkit.org/show_bug.cgi?id=11141">bug 11141</a>: 
        CSS '@import' doesn't respect HTML Base element.</p>
        <p class="c1">This text should be green.</p>
        <p>If it is red, the css has been loaded relative to the document.
        If it is black, no stylesheet has been rendered, if it is rendered green,
        the stylesheet has been rendered correctly from the HREF attribute of the
        Base element in the HEAD section of this document.</p>
        <p class="c2">This text should also be green.</p>
    </body>
</html>
