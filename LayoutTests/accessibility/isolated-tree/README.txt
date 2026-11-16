Safari on macOS runs with "isolated tree mode" enabled, whereas embedded WebKit on macOS
and other platforms all run without that mode, which we call "live tree mode".

To ensure test coverage, we have a duplicate of nearly every test in this isolated-tree
directory to ensure that running layout tests covers both modes.

The only difference is that these tests should have a directive at the top to enable
isolated tree mode, and relative paths may be updated. In rare cases expectations might
be subtly different, but in general that should be considered a bug and expectations
should be aligned. However, slightly wrong expectations is better than no coverage at
all.
