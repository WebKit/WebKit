# Pull Request Template

## File a Bug

All changes should be associated with a bug. The WebKit project is currently using [Bugzilla](https://bugs.webkit.org) as our bug tracker. Note that multiple changes may be associated with a single bug.

## Provided Tooling

The WebKit Project strongly recommends contributors use [`Tools/Scripts/git-webkit`](https://github.com/WebKit/WebKit/tree/main/Tools/Scripts/git-webkit) to generate pull requests. See [Setup](https://github.com/WebKit/WebKit/wiki/Contributing#setup) and [Contributing Code](https://github.com/WebKit/WebKit/wiki/Contributing#contributing-code) for how to do this.

## Template

If a contributor wishes to file a pull request manually, the template is below. Manually-filed pull requests should contain their commit message as the pull request description, and their commit message should be formatted like the template below.

Additionally, the pull request should be mentioned on [Bugzilla](https://bugs.webkit.org), labels applied to the pull request matching the component and version of the [Bugzilla](https://bugs.webkit.org) associated with the pull request and the pull request assigned to its author.

<pre>
< bug title >
<a href="https://bugs.webkit.org/enter_bug.cgi">https://bugs.webkit.org/show_bug.cgi?id=#####</a>

Reviewed by NOBODY (OOPS!).

Explanation of why this fixes the bug (OOPS!).

* path/changed.ext:
(function):
(class.function):

</pre>
