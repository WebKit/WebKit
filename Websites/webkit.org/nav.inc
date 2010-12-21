<div id="nav">
    <ul id="navlist">
        <li><a href="/">Home</a></li>
        <li><a href="/blog/">Surfin&#8217; Safari Blog</a></li>
        <li><a href="http://planet.webkit.org/">Planet WebKit</a></li>
        <li><a href="/projects/goals.html">Project Goals</a></li>
        <li><a href="/contact.html">Keeping in Touch</a></li>
        <li><a href="http://trac.webkit.org/">Trac</a></li>
        <li><a href="/meeting/">Contributors Meeting</a></li>

        <li class="subtitle">Working with the Code</li>

        <li><a href="/building/tools.html">Installing Developer Tools</a></li>
        <li><a href="/building/checkout.html">Getting the Code</a></li>
        <li><a href="/building/build.html">Building WebKit</a></li>
        <li><a href="/building/run.html">Running WebKit</a></li>
        <li><a href="/building/debug.html">Debugging WebKit</a></li>
        <li><a href="/coding/contributing.html">Contributing Code</a></li>
        <li><a href="/coding/commit-review-policy.html">Commit and Review Policy</a></li>
        <li><a href="/security/">Security Policy</a></li>

        <li class="subtitle">Documentation</li>

        <li><a href="http://trac.webkit.org/wiki">Wiki</a></li>
        <li><a href="/projects/">Projects</a></li>
        <li><a href="/coding/coding-style.html">Code Style Guidelines</a></li>
        <li><a href="/coding/technical-articles.html">Technical Articles</a></li>
        <li><a href="http://trac.webkit.org/wiki/WebInspector">Web Inspector</a></li>
        <li><a href="http://trac.webkit.org/wiki/WebDevelopers">Web Developer Resources</a></li>
        <li><a href="/demos/">Demos</a></li>

        <li class="subtitle">Testing</li>

        <li><a href="/quality/testing.html">Regression Testing</a></li>
        <li><a href="/quality/leakhunting.html">Leak Hunting</a></li>
        <li><a href="/quality/testwriting.html">Writing New Tests</a></li>
        <li><a href="/quality/crashlogs.html">Getting a Crash Log</a></li>

        <li class="subtitle">Bugs</li>

        <li><a href="/quality/reporting.html">Reporting Bugs</a></li>
        <li><a href="/quality/bugwriting.html">Bug Report Guidelines</a></li>
        <li><a href="/quality/bugpriorities.html">Bug Prioritization</a></li>
        <li><a href="/quality/reduction.html">Test Case Reduction</a></li>
        <li><a href="/quality/lifecycle.html">Bug Life Cycle</a></li>
<?php
        if (function_exists("wp_get_archives")) {
            print "<li class=\"subtitle\">Archives</li>\n";
            wp_get_archives('type=monthly');
        }
?>
    </ul>
    <div id="notice">
        WebKit is open source software with portions licensed under the
        <a href="/coding/lgpl-license.html">LGPL</a> and
        <a href="/coding/bsd-license.html">BSD licenses</a>.
        Complete license and copyright information can be found within the code.
    </div>

    <div id="terms">
        Hosting provided by <a href='http://www.macosforge.org/'>Mac OS Forge</a>.
        Use of this site is subject to the
        <a href='http://www.macosforge.org/terms/'>Mac OS Forge Terms of Use</a>.
    </div>
</div>
