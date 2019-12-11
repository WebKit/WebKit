# How to change BoringSSL's API

BoringSSL has more flexibility in changing things than many other library projects because we have a reasonable idea of who our users are. Still, breaking changes require some care. We depend on tight feedback loops with our consumers so that we can learn about mistakes and fix them. For that to work, updating BoringSSL must be smooth.

Ultimately, the strategy for each breaking change is decided on a case-by-case basis. This document provides guidelines and techniques to help with a smooth transition.

## Breakage risk

Traditionally, breaking changes are defined in terms of API or ABI surface. Exposed symbols and type signatures cannot change, etc. But this is a poor approximation of the true impact. Removing an API may not a breaking change if no one is using it. Conversely, [Hyrum's Law](http://www.hyrumslaw.com/) applies. Fixing a bug may be a breaking change for some consumer which was depending on that bug.

Thus, we do not think about whether a change is formally a breaking change, but about the *risk* of it breaking someone.

Some changes, such as internal cleanups or bug-fixes, are low risk and do not need special measures. Any problems can be handled when the affected consumer updates BoringSSL and notices.

Other changes, such as removing an API, forbidding some edge case, or adjusting some behavior, are more likely to break things. To help the consumer triage any resulting failures, include some text in the commit message, prefixed by `Update-Note: `. This can include what this change may break and instructions on how to fix the issue.

## Code Search

The vast majority of BoringSSL consumers are conveniently indexed in various Code Search instances. This can predict the impact of a risky change and identify code to fix ahead of time. The document &ldquo;How to Code Search&rdquo; in the (Google-only) [go/boringssl-folder](https://goto.google.com/boringssl-folder) includes notes on this.

## Evaluate a change's cost

If some change has high cost (from having to fix consumers) and relatively little benefit to BoringSSL, it may not be worth the trouble. For instance, it is likely not worth removing a small compatibility function in the corner of the library that is easily dropped by the static linker.

Conversely, a change that leads to a major improvement to all BoringSSL consumers, at the cost of fixing one or two consumers, is typically worth it.

## Fixing consumers

If code search reveals call sites that are definitely going to break, prefer to handle these before making the change. While unexpected breakage is always possible, we generally consider it the responsibility of the developer or group making a change to handle impact of that change. Teams are generally unhappy to be surprised by new migration work but happy to have migration work done for them.

In most cases, this is straightforward:

1. Add the replacement API.
2. As the replacement API enters each consuming repository, migrate callers to it.
3. Remove the original API once all consumers have been migrated.

The removal should still include an `Update-Note` tag, in case some were missed.

In some cases, this kind of staged approach is not feasible: perhaps the same code cannot simultaneously work before and after the change, or perhaps there are too many different versions in play. For instance, [Conscrypt](https://github.com/google/conscrypt) feeds into three different repositories. The GitHub repository consumes BoringSSL's `master` branch directly. It is pushed into Android, where it consumes Android's `external/boringssl`. Yet another copy is pushed into the internal repository, where it consumes that copy of BoringSSL. As each of these Conscrypts are updated independently from their corresponding BoringSSLs, Conscrypt upstream cannot rely on a new BoringSSL API until it is present in all copies of BoringSSL its downstreams rely on.

In that case, a multi-sided change may be more appropriate:

1. Upload the breaking change to Gerrit, but do not submit it yet. Increment the `BORINGSSL_API_VERSION` symbol.
2. Update the consuming repository with `#if BORINGSSL_API_VERSION < N` preprocessor logic. Leave a comment to remove this later, linking to your BoringSSL change.
3. When the `BORINGSSL_API_VERSION` check has propagated to relevant copies of the consuming repository, submit the BoringSSL change.
4. When the BoringSSL change has propagated to relevant copies of BoringSSL, remove the staging logic from the consumer.

Finally, in some cases, the consumer's change may be committed atomically with the BoringSSL update. This can only be done for code which only consumes one instance of BoringSSL (so the Conscrypt example above is not eligible). Check with that project's maintainer first or, better, be that project's maintainer.

If more complex changes are needed in some consumer, communicate with the relevant maintainers to plan the transition.

## Fail early, fail closed

When breaking changes do occur, they should fail as early and as detectably as possible.

Ideally, problematic consumers fail to compile. Prefer to remove functions completely over leaving an always failing stub function. Sometimes this is not possible due to other consumers, particularly bindings libraries. Alternatively, if a stub function can be reasonably justified as still satisfying the API constraints, consider adding one to improve compatibility. For example, BoringSSL has many no-op stubs corresponding to OpenSSL's many initialization functions.

If some parameter now must be `NULL`, change the type to an opaque struct pointer. Consumers passing non-`NULL` pointers will then fail to compile.

If breaking the compile is not feasible, break at runtime, in the hope that consumers have some amount of test coverage. When doing so, try to fail on the common case. In particular, do not rely on consumers adequately testing or even checking for failure cases. One strategy is to bring the object into a &ldquo;poison&rdquo; state: if an illegal operation occurs, set a flag to fail all subsequent ones.

In other functions, it may be appropriate to simply call `abort()`.

## Unexpected breakage

While we try to avoid breaking things, sometimes things unexpectedly break. Depending on the impact, we may fix the consumer, make a small fix to BoringSSL, or revert the change to either try again later or revise the approach.

If we do not ultimately fix the consumer, add a test in BoringSSL to capture the unexpected API contract, so future regressions are caught quickly.

## Canary changes and bake time

When planning a large project that depends on a breaking change, prefer to make the breaking change first&mdash;before committing larger changes. Or, when changing toolchain or language requirements, add a small instance of the dependency somewhere first then wait a couple of weeks for the change to appear in consumers. This ensures that reverting the change is still feasible if necessary.

While we rely on a tight feedback loop with our consumers, there are a few consumers which update less frequently. For extremely risky changes, such as introducing C++ to a target, it may be prudent to wait much longer.

## Third-party code

In many cases, we are interested in changing behavior which came from OpenSSL. OpenSSL's API surface is huge, but only a small subset is actually used. So we can and occasionally do change these behaviors. This is more complex than changing BoringSSL-only behavior due to third-party code.

We use BoringSSL with many third-party projects that normally use OpenSSL. Generally, we consider this our burden to make this work and do not encourage external projects to depend on BoringSSL. While we can and do maintain patches for this as necessary, it has overhead and so the cost of breaking third-party code is higher.

We lean fairly strongly towards making changes to BoringSSL over patching third-party code, unless the third-party change fixes a security problem.

Additionally, changing an OpenSSL API will not only affect third-party code we use today, but also any third-party code we use in the future. Thus Code Search is less useful as an absolute predictor, and the various other considerations in this document are more important.

If the patch to support a BoringSSL change can be generally useful to the third-party project, send it upstream. For instance, it may use the APIs better, clean up code, or help support newer versions of OpenSSL. In general, we try to target compatibility with &ldquo;most&rdquo; &ldquo;well-behaved&rdquo; OpenSSL consumers.

Finally, if some particular OpenSSL API or pattern is problematic to BoringSSL, it is likely problematic to OpenSSL too. Consider filing a bug with them to suggest a change, either in new code going forward or for the next API break. OpenSSL's release cycles and feedback loops are much longer than BoringSSL's, so this is usually not immediately useful, but it keeps the ecosystem moving in the right direction.
