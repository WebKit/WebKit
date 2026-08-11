This is the copy of W3C's CSS2.1 test suite.

http://www.w3.org/Style/CSS/Test/CSS2.1/20061011/

We should not add files into this directory and should not modify the
files in this directory. If you find invalid test cases in this
directory, report the problem to W3C and update the files of this
directory after they fixed the issue.

* About t1204-(increment|reset)-

These 6 tests produce wrong results with DumpRenderTree because they
use setTimeout but there are no waitUntilDone call. We disabled them
in this directory and have modified version of them as
fast/css/counters/counter-(increment|reset)-* .
