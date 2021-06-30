
.. _`tmpdir handling`:
.. _tmpdir:

Temporary directories and files
================================================

The ``tmp_path`` fixture
------------------------




You can use the ``tmp_path`` fixture which will
provide a temporary directory unique to the test invocation,
created in the `base temporary directory`_.

``tmp_path`` is a ``pathlib/pathlib2.Path`` object. Here is an example test usage:

.. code-block:: python

    # content of test_tmp_path.py
    import os

    CONTENT = "content"


    def test_create_file(tmp_path):
        d = tmp_path / "sub"
        d.mkdir()
        p = d / "hello.txt"
        p.write_text(CONTENT)
        assert p.read_text() == CONTENT
        assert len(list(tmp_path.iterdir())) == 1
        assert 0

Running this would result in a passed test except for the last
``assert 0`` line which we use to look at values:

.. code-block:: pytest

    $ pytest test_tmp_path.py
    =========================== test session starts ============================
    platform linux -- Python 3.x.y, pytest-6.x.y, py-1.x.y, pluggy-0.x.y
    cachedir: $PYTHON_PREFIX/.pytest_cache
    rootdir: $REGENDOC_TMPDIR
    collected 1 item

    test_tmp_path.py F                                                   [100%]

    ================================= FAILURES =================================
    _____________________________ test_create_file _____________________________

    tmp_path = PosixPath('PYTEST_TMPDIR/test_create_file0')

        def test_create_file(tmp_path):
            d = tmp_path / "sub"
            d.mkdir()
            p = d / "hello.txt"
            p.write_text(CONTENT)
            assert p.read_text() == CONTENT
            assert len(list(tmp_path.iterdir())) == 1
    >       assert 0
    E       assert 0

    test_tmp_path.py:13: AssertionError
    ========================= short test summary info ==========================
    FAILED test_tmp_path.py::test_create_file - assert 0
    ============================ 1 failed in 0.12s =============================

.. _`tmp_path_factory example`:

The ``tmp_path_factory`` fixture
--------------------------------




The ``tmp_path_factory`` is a session-scoped fixture which can be used
to create arbitrary temporary directories from any other fixture or test.

It is intended to replace ``tmpdir_factory``, and returns :class:`pathlib.Path` instances.

See :ref:`tmp_path_factory API <tmp_path_factory factory api>` for details.


The 'tmpdir' fixture
--------------------

You can use the ``tmpdir`` fixture which will
provide a temporary directory unique to the test invocation,
created in the `base temporary directory`_.

``tmpdir`` is a `py.path.local`_ object which offers ``os.path`` methods
and more.  Here is an example test usage:

.. code-block:: python

    # content of test_tmpdir.py
    import os


    def test_create_file(tmpdir):
        p = tmpdir.mkdir("sub").join("hello.txt")
        p.write("content")
        assert p.read() == "content"
        assert len(tmpdir.listdir()) == 1
        assert 0

Running this would result in a passed test except for the last
``assert 0`` line which we use to look at values:

.. code-block:: pytest

    $ pytest test_tmpdir.py
    =========================== test session starts ============================
    platform linux -- Python 3.x.y, pytest-6.x.y, py-1.x.y, pluggy-0.x.y
    cachedir: $PYTHON_PREFIX/.pytest_cache
    rootdir: $REGENDOC_TMPDIR
    collected 1 item

    test_tmpdir.py F                                                     [100%]

    ================================= FAILURES =================================
    _____________________________ test_create_file _____________________________

    tmpdir = local('PYTEST_TMPDIR/test_create_file0')

        def test_create_file(tmpdir):
            p = tmpdir.mkdir("sub").join("hello.txt")
            p.write("content")
            assert p.read() == "content"
            assert len(tmpdir.listdir()) == 1
    >       assert 0
    E       assert 0

    test_tmpdir.py:9: AssertionError
    ========================= short test summary info ==========================
    FAILED test_tmpdir.py::test_create_file - assert 0
    ============================ 1 failed in 0.12s =============================

.. _`tmpdir factory example`:

The 'tmpdir_factory' fixture
----------------------------



The ``tmpdir_factory`` is a session-scoped fixture which can be used
to create arbitrary temporary directories from any other fixture or test.

For example, suppose your test suite needs a large image on disk, which is
generated procedurally. Instead of computing the same image for each test
that uses it into its own ``tmpdir``, you can generate it once per-session
to save time:

.. code-block:: python

    # contents of conftest.py
    import pytest


    @pytest.fixture(scope="session")
    def image_file(tmpdir_factory):
        img = compute_expensive_image()
        fn = tmpdir_factory.mktemp("data").join("img.png")
        img.save(str(fn))
        return fn


    # contents of test_image.py
    def test_histogram(image_file):
        img = load_image(image_file)
        # compute and test histogram

See :ref:`tmpdir_factory API <tmpdir factory api>` for details.


.. _`base temporary directory`:

The default base temporary directory
-----------------------------------------------

Temporary directories are by default created as sub-directories of
the system temporary directory.  The base name will be ``pytest-NUM`` where
``NUM`` will be incremented with each test run.  Moreover, entries older
than 3 temporary directories will be removed.

You can override the default temporary directory setting like this:

.. code-block:: bash

    pytest --basetemp=mydir

.. warning::

    The contents of ``mydir`` will be completely removed, so make sure to use a directory
    for that purpose only.

When distributing tests on the local machine using ``pytest-xdist``, care is taken to
automatically configure a basetemp directory for the sub processes such that all temporary
data lands below a single per-test run basetemp directory.

.. _`py.path.local`: https://py.readthedocs.io/en/latest/path.html
