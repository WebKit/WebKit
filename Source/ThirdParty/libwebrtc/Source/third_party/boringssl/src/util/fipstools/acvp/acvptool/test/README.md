# ACVP unit tests

This directory contains tests for the ACVP infrastructure itself. These tests
consist of test vector files and their expected outputs. The test vectors are
stored as bzip2 compressed JSON in the `vector/` directory, and the
corresponding expected output is the file with the same name in the `expected/`
directory.

`tests.json` contains a list of all the tests: their input and output files, and
the module wrapper to use with them. Some tests use the standard BoringSSL
module wrapper, which we use for ACVP validations, called `modulewrapper`. But
some tests use algorithms that we don't support in BoringSSL and, for those,
there's a special test-only module wrapper written in Go called
`testmodulewrapper`.

## Adding tests

When adding support for a new algorithm, the process for adding tests is:

1.  Go to the parent directory of this file. (I.e. `acvptool/`.)
2.  Get the implementation working such that NIST's server accepts the results.
3.  Generally fetch a set of test vectors from the NIST demo server. Access to
    the demo server requires permission from NIST, although you can also ask an
    NVLAP lab to fetch a demo set of vectors for you. The vectors are generally
    huge—larger than we would want to check in, even compressed—so
    `test/trim_vectors.go` can be used to keep only a (hopefully valid) subset
    of them.
4.  Copy the trimmed vectors into `test/vectors/`, named after the algorithm and
    without any `json` suffix.
5.  Run `bzip2 -9 test/vectors/ALGO`. This will produce the `.bz2` file. Delete
    the original.
6.  Run `./acvptool -json /tmp/trimmed_vectors.json > test/expected/ALGO` in the
    parent directory to generate the expected results. (If you need to use the
    test-only module wrapper then use the `-wrapper` argument.)
7.  Run `bzip2 -9 test/expected/ALGO` and delete the original.
8.  Update `test/tests.json` to include the new test.
9.  Run `ninja -C build run_tests` in the top-level directory to ensure that
    everything is working.
