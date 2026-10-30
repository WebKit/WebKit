# WebDriver tests

## Importing tests

There is the `import-webdriver-tests` script to automate most of the process of
checking out the repository and copying the files.

It reads the `importer.json` of the selected suite (selenium or w3c), which
contains the desired commit alongside the list of paths to skip/import.

The cloned repository sits in `WebKitBuild/` for easier inspection. For example,
to manually check the new commit to be imported.

Once chosen the new commit, update `importer.json` with its hash and
eventual path changes (e.g., new folders to be copied or skipped) and run the
import script to update the desired suite:

```
./Tools/Scripts/import-webdriver-tests --selenium`
./Tools/Scripts/import-webdriver-tests --w3c`
```

After running the script, if you're using git, you can check which files
were added with `git status WebDriverTests/` to add them to the new commit.
One current limitation of the script is its inability to check for deleted
files from the source repository, so this step is still manual.

Beware that when importing Selenium tests, some manual intervention might be
needed to remove unsupported code related to Firefox, Chrome, etc. For example:

* Package imports in `WebDriverTests/imported/selenium/py/selenium/webdriver/__init__.py`.
* `RemoteConnection` objects in `WebDriverTests/imported/selenium/py/selenium/webdriver/remote/webdriver.py`.


To test the imported suite, run it:

```
Tools/Scripts/run-webdriver-tests --verbose --wpe --release --display-server=xvfb
```

## Expectations

`TestExpectations.json` lists the tests that don't simply pass. Each test file can have an `expected` entry for
the whole file, and entries for its subtests, which take precedence:

```json
"imported/w3c/webdriver/tests/classic/navigate_to/navigate.py": {
    "expected": {"all": {"status": ["FAIL"], "bug": "webkit.org/b/123456"}},
    "subtests": {
        "test_basic": {
            "expected": {"glib": {"status": ["PASS"]}, "mac": {"status": ["TIMEOUT"], "note": "Why it times out"}}
        }
    }
}
```

The keys of an `expected` object say which ports an entry is for. For each port, `run-webdriver-tests` looks for
its full name (for example `mac-tahoe-wk2`), then its base name (`mac`, `gtk` or `wpe`), then its group (`cocoa`
for Mac; `glib` for GTK and WPE), then `all`. Each of these can be qualified with a build type, an architecture,
or both, as in `gtk@Debug`, `all:arm64` or `mac@Release:arm64`, and a qualified key is preferred to an unqualified
one at the same level. The first key that matches is used as a whole: its fields aren't combined with those of
other keys.

`status` lists the results that are expected, more than one for a flaky test: `PASS`, `FAIL`, `TIMEOUT` or
`SKIP`. `slow` gives a test five times the usual timeout. `bug` and `note` are for people.
