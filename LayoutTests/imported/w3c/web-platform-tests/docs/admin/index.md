# Project Administration

This section documents all the information necessary to administer the
infrastructure which makes the project possible.

## Tooling

```eval_rst
.. toctree::
   :titlesonly:

   ../README
   ../tools/wptrunner/README
   ../tools/wptserve/docs/index.rst
   pywebsocket3

.. toctree::
   :hidden:

   ../tools/wptserve/README
   ../tools/third_party/pywebsocket3/README
```

### Indices and tables

```eval_rst
* :ref:`modindex`
* :ref:`genindex`
* :ref:`search`
```

## Secrets

SSL certificates for all HTTPS-enabled domains are retrieved via [Let's
Encrypt](https://letsencrypt.org/), so that data does not represent an
explicitly-managed secret.

## Third-party account owners

- (unknown registrar): https://web-platform-tests.org
  - jgraham@hoppipolla.co.uk
- (unknown registrar): https://w3c-test.org
  - mike@w3.org
- (unknown registrar): http://testthewebforward.org
  - web-human@w3.org
- [Google Domains](https://domains.google/): https://wpt.fyi
  - foolip@google.com
  - robertma@google.com
  - mike@bocoup.com
- (Google internal): https://wpt.live https://wptpr.live
  - foolip@google.com
  - robertma@google.com
- [GitHub](https://github.com/): web-platform-tests
  - [@foolip](https://github.com/foolip)
  - [@Hexcles](https://github.com/Hexcles)
  - [@jgraham](https://github.com/jgraham)
  - [@plehegar](https://github.com/plehegar)
  - [@thejohnjansen](https://github.com/thejohnjansen)
  - [@youennf](https://github.com/youennf)
  - [@zcorpan](https://github.com/zcorpan)
- [GitHub](https://github.com/): w3c
  - [@plehegar](https://github.com/plehegar)
  - [@sideshowbarker](https://github.com/sideshowbarker)
- [Google Cloud Platform](https://cloud.google.com/): wptdashboard{-staging}
  - robertma@google.com
  - smcgruer@google.com
  - foolip@google.com
- [Google Cloud Platform](https://cloud.google.com/): wpt-live
  - smcgruer@google.com
  - robertma@google.com
- [Google Cloud Platform](https://cloud.google.com/): wpt-pr-bot
  - smcgruer@google.com
  - robertma@google.com
- E-mail address: wpt.pr.bot@gmail.com
  - smcgruer@google.com
  - boaz@bocoup.com
  - mike@bocoup.com
  - simon@bocoup.com
- [GitHub](https://github.com/): @wpt-pr-bot account
  - smcgruer@google.com
  - boaz@bocoup.com
  - mike@bocoup.com
  - simon@bocoup.com

[web-platform-tests]: https://github.com/e3c/web-platform-tests
[wpt.fyi]: https://github.com/web-platform-tests/wpt.fyi

## Emergency playbook

### Lock down write access to the repo

**Recommended but not yet verified approach:** Create a [new branch protection
rule](https://github.com/web-platform-tests/wpt/settings/branch_protection_rules/new)
that applies to `*` (i.e. all branches), and check "Restrict who can push to
matching branches". This should prevent everyone except those with the
"Maintain" role (currently only the GitHub admins listed above) from pushing
to *any* branch. To lift the limit, delete this branch protection rule.

**Alternative approach proven to work in
[#21424](https://github.com/web-platform-tests/wpt/issues/21424):** Go to
[manage access](https://github.com/web-platform-tests/wpt/settings/access),
and change the permission of "reviewers" to "Read". To lift the limit, change
it back to "Write". This has the known downside of *resubscribing all reviewers
to repo notifications*.
