# Cutting Periodic "Releases"

The [Bazel Central Registry](https://github.com/bazelbuild/bazel-central-registry)
needs versioned snapshots and cannot consume git revisions directly. To cut a
release, do the following:

1. Pick a new version. The current scheme is `0.YYYYMMDD.0`. If we need to cut
   multiple releases in one day, increment the third digit.

2. Update `MODULE.bazel` with the new version and upload to Gerrit.

3. Once that CL lands, make a annotated git tag at the revision. This can be
   [done from Gerrit](https://boringssl-review.googlesource.com/admin/repos/boringssl,tags).
   The "Annotation" field must be non-empty. (Just using the name of the tag
   again is fine.)

4. Wait for the tag to be mirrored to GitHub. If this takes too long, fetch the
   tag in your local checkout and push it by hand. (`git fetch origin`
   followed by `git push github NAME_OF_TAG`, if `github` is the name of the
   remote in your checkout.)

5. Download the tarball of the tag [from GitHub](https://github.com/google/boringssl/tags)
   using the "tar.gz" link.

6. Create a corresponding GitHub
   [release](https://github.com/google/boringssl/releases/new). You have to be
   an owner of GitHub repository for this to work. Ask in the team chat if it
   does not work. Attach the tarball to the release. (The `prepare_bcr_module`
   tool will check that this was correct.)

7. Clone a copy of https://github.com/bazelbuild/bazel-central-registry so that you
   can make a GitHub pull request against it.

8. Run `go run ./util/prepare_bcr_module TAG` and follow the instructions. The
   tool does not require special privileges, though it does fetch URLs from
   GitHub and read the local checkout. It outputs a JSON file for BCR's tooling
   to consume. The instructions will tell you to run a bazelisk command.

9. CD into the root of your bazel-central-registry fork, and run the bazelisk
   command indicated by the script output above. It will add a directory to the
   repository which is untracked, use `git status` to find it, and `git add` to
   add it to what you are about to commit.  Then commit the changes and added
   directory, and submit it as a pr to bazel-central-registry.
