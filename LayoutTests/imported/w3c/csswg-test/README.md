W3C CSS Test Suite Repository
-----------------------------

This repository contains top level directories for all of CSS specs for
which we currently have tests. Place tests in the appropriate directory based
on the first rel="help" link in the test. If you are writing tests for a spec
and it doesn't yet have a directory here, feel free to create it.

There are a few directories that do not map to specifications:

support/ contains common image files to which many of the tests link in
this location

tools/ is random scripts that may be useful when administering tests.

vendor-imports/ is where third parties may import their tests that originate
and are maintained in an external repo. Files in this directory should
never be modified in this repo, but should go through the vendor's process
to be imported here.

work-in-progress/ is a legacy directory that contains all the work that was
once submitted to the repo, but was not yet ready for review. Since the CSSWG
has adopted the GitHub pull request process, no new files should be landed here.
The subdirectories here are named by test author or contributing organization.

Running the Tests
-----------------

The tests are designed to be run from your local computer. The test
environment requires Python 2.7+ (but not Python 3.x). You will also
need a copy of OpenSSL. Users on Windows should read the
[Windows Notes](#windows-notes) section below.

To get the tests running, you need to set up the test domains in your
[`hosts` file](http://en.wikipedia.org/wiki/Hosts_%28file%29%23Location_in_the_file_system). The
following entries are required:

```
127.0.0.1   csswg.test
127.0.0.1   www.csswg.test
127.0.0.1   www1.csswg.test
127.0.0.1   www2.csswg.test
127.0.0.1   xn--n8j6ds53lwwkrqhv28a.csswg.test
127.0.0.1   xn--lve-6lad.csswg.test
0.0.0.0     nonexistent-origin.csswg.test
```

Because csswg-test uses git submodules, you must ensure that
these are up to date. In the root of your checkout, run:

```
git submodule update --init --recursive
```

The test environment can then be started using

    ./serve

This will start HTTP servers on two ports and a websockets server on
one port. By default one web server starts on port 8000 and the other
ports are randomly-chosen free ports. Tests must be loaded from the
*first* HTTP server in the output. To change the ports, copy the
`config.default.json` file to `config.json` and edit the new file,
replacing the part that reads:

```
"http": [8000, "auto"]
```

to some port of your choice e.g.

```
"http": [1234, "auto"]
```

If you installed OpenSSL in such a way that running `openssl` at a
command line doesn't work, you also need to adjust the path to the
OpenSSL binary. This can be done by adding a section to `config.json`
like:

```
"ssl": {"openssl": {"binary": "/path/to/openssl"}}
```
Windows Notes
-------------

Running wptserve with SSL enabled on Windows typically requires
installing an OpenSSL distribution.
[Shining Light](http://slproweb.com/products/Win32OpenSSL.html)
provide a convenient installer that is known to work, but requires a
little extra setup.

After installation ensure that the path to OpenSSL is on your `%Path%`
environment variable.

Then set the path to the default OpenSSL configuration file (usually
something like `C:\OpenSSL-Win32\bin\openssl.cfg` in the server
configuration. To do this copy `config.default.json` in the
web-platform-tests root to `config.json`. Then edit the JSON so that
the key `ssl/openssl/base_conf_path` has a value that is the path to
the OpenSSL config file.

Linking Your Tests to Specifications
-----------------------------------

In addition to placing your tests in the appropriate directory in this repository,
you must also include at least one specification link in the test metadata,
following [these guidelines][speclinks].

For CSS tests, you must also be sure you’re linking to a specific level of the spec,
generally the first level where the feature being tested is defined. Where possible,
it’s preferable to link to the official version of the spec, which will start with
https://www.w3.org/TR/. This can usually be found as the ‘Latest version’ link in the
spec itself and will include the level of the spec in the URL. For example, the proper
link to level 1 of the CSS Flexbox spec is:

https://www.w3.org/TR/css-flexbox-1/#RELEVANT_SECTION

When testing features not yet available in an official draft, link to the appropriate
Editor’s Draft found at https://drafts.csswg.org/. Be sure to include the level of the
specification in the link. For example, the proper link to the CSS Flexbox Level 1
Editor’s Draft is:

https://drafts.csswg.org/css-flexbox-1/#RELEVANT_SECTION

Contributing
-------------

Absolutely everyone is welcome (and even encouraged) to contribute to test
development, so long as you fulfill the contribution requirements detailed
in the [Contributing Guidelines][contributing]. No test is too small or too
simple, especially if it corresponds to something for which you've noted an
interoperability bug in a browser.

Getting Involved
----------------

If you wish to contribute actively, you're very welcome to join the
public-css-testsuite@w3.org mailing list by
[signing up to our mailing list](mailto:public-css-testsuite-request@w3.org?subject=subscribe).
The mailing list is [archived][mailarchive].

[mailarchive]: https://lists.w3.org/Archives/Public/public-css-testsuite/

Write Access
------------

This section only applies if you have cloned the repository from
Mercurial. If you've cloned it from GitHub, which is a mirror of
the canonical Mercurial repo, you can submit your tests via a [pull request][github101].

To gain write access to this Mercurial repository, sign up for an account
on the CSS Test Suite Manager (aka Shepherd) at:
https://test.csswg.org/shepherd/register
and then submit a request on the Repository Access page at:
https://test.csswg.org/shepherd/account/access/

You will be notified by email when your request is processed.

Please note that although we will grant write access directly to the Mercurial
repo, it is strongly advised to use GitHub for test submissions to enable
reviewers to use its built-in review tools. Direct submissions to Mercurial
should be limited to administrative or housekeeping tasks, very minor changes
that don't require a review, or from advanced users of the system.

[contributing]: https://github.com/w3c/csswg-test/blob/master/CONTRIBUTING.md
[github101]: http://testthewebforward.org/docs/github-101.html
[speclinks]: http://testthewebforward.org/docs/test-templates.html#specification-links
