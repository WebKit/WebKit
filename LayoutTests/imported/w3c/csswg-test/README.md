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
