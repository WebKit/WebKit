### Runtime flags

Generally, new author-facing features should be implemented behind **runtime flags**. Initially, such flags should be disabled on trunk. As work on a feature matures, its flag may be enabled on trunk. Eventually, runtime flags guarding widely deployed, stable features should be removed.

The following criteria define when runtime flags should be enabled or disabled **on trunk**. This policy is not intended to embody or communicate a feature release policy for any particular WebKit port or product which embeds WebKit. It is the responsibility of each port to determine if or when runtime flags should be enabled or disabled for release. Ports are encouraged to document any additional criteria they may use.

In some cases, compile time flags should be used in addition to or instead of runtime flags:

* When merely compiling a feature in significantly impacts the hackability or livability of trunk.
* When a feature requires a platform-specific backend that is not available on all platforms.
* When some ports or platforms have resource constraints which require the ability to remove the feature completely from builds.
* When a feature is otherwise only relevant to some ports or platforms.

That said, runtime flags are preferred whenever feasible.

A runtime flag should be **disabled on trunk**:

* When work on the feature begins.
* When the feature is not intended to be Web-exposed.
* When the feature is defined in a Web standard which is immature or is likely to incompatibly change.
* If enabling the feature would negatively affect the livability of trunk. (In such cases a compile-time flag may also be warranted.)

Later, a runtime flag may be **enabled on trunk**:

* When the feature is defined in a mature Web standard which is unlikely to incompatibly change.
* If the implementation of the feature is relatively mature and it is ready for wider testing or review.
* If support for the feature is required for Web compatibility.

Eventually, a runtime flag may be removed and the feature made **always on**:

* When most or all major ports of WebKit are shipping the feature and none plan to turn it off.

### Naming

In general, new features **should not be prefixed**.

A new feature **should be prefixed** if the feature is not intended to be Web-exposed. (And, as established in the previous section, such a feature's runtime flag should be disabled on trunk.)

A prefix specific to the product or content type is preferred over the `-webkit-` prefix. For example, a feature intended for FooCorp's flagship Foo product should get a `-foo-` prefix, not a `-webkit-` one.
