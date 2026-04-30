# webkitexpectationspy

A pluggable test expectations management system for WebKit.

## Overview

`webkitexpectationspy` provides a unified system for parsing, querying, and linting test expectation files across WebKit's test infrastructure. It supports:

- **API tests** (TestWebKitAPI, TestIPC, TestWGSL, TestWTF)
- **Layout tests** (LayoutTests) with ImageOnlyFailure, Text, Audio, Leak expectations
- **Custom formats** via plugins

## Features

- **Unified format**: One consistent syntax across all test types
- **Version specifiers**: `Sonoma+`, `Ventura-`, `Monterey-Sonoma`
- **Configuration categories**: Platform, Version, Style, Hardware, Architecture, Flavor
- **Smart linting**: Category ordering, alphabetical ordering, combination collapse detection
- **Auto-fix**: Automatic correction of common issues
- **Plugin architecture**: Extend for custom test formats

## Installation

```bash
pip install webkitexpectationspy
```

Or for development:

```bash
cd Tools/Scripts/libraries/webkitexpectationspy
pip install -e .
```

## Quick Start

```python
from webkitexpectationspy import ExpectationsManager
from webkitexpectationspy.plugins import APITestPlugin

# Create manager with API test plugin
manager = ExpectationsManager(plugin=APITestPlugin())

# Load expectations file
manager.load_file('TestExpectations')

# Query expectations
exp = manager.get_expectation('TestWebKitAPI.WTF.StringTest')
if exp and exp.is_skip():
    print('Test is skipped')

# Get skipped tests for current configuration
skipped = manager.get_skipped_tests(
    all_tests,
    current_config={'mac', 'debug', 'arm64'}
)
```

### Layout Tests Example

```python
from webkitexpectationspy import ExpectationsManager
from webkitexpectationspy.plugins import LayoutTestPlugin

# Create manager with layout test plugin
manager = ExpectationsManager(plugin=LayoutTestPlugin())

# Load expectations file
manager.load_file('LayoutTests/TestExpectations')

# Query expectations - supports directory wildcards
exp = manager.get_expectation('fast/css/border-radius.html')

# Layout test-specific expectations
# ImageOnlyFailure, Text, Audio, Leak
```

## Expectation File Format

```
# Basic format
[bug-ids] [configurations] test-pattern [expectations]

# Examples
TestWebKitAPI.WTF.StringTest [ Pass ]
webkit.org/b/12345 [ mac Sonoma+ ] TestWebKitAPI.WebKit.Bug [ Fail ]
rdar://98765 [ ios simulator ] TestWebKitAPI.iOS.Test [ Skip ]
[ debug arm64 ] TestWebKitAPI.WTF.* [ Slow:120s ]
```

### Configuration Categories (ordered)

1. **Platform**: mac, ios, linux, win, gtk, wpe
2. **Version**: Sonoma, iOS18, with `+`/`-`/range modifiers
3. **Style**: debug, release, asan, guardmalloc
4. **Hardware**: simulator, device, iphone, ipad
5. **Architecture**: arm64, x86_64, x86, arm64_32, armv7k
6. **Flavor**: wk1, wk2, siteisolation, etc. (freeform)

### Version Specifiers

| Syntax | Meaning |
|--------|---------|
| `Sonoma` | Exact version only |
| `Sonoma+` | This version and later |
| `Sonoma-` | This version and earlier |
| `Ventura-Sequoia` | Inclusive range |

### Bug Identifiers

- `webkit.org/b/12345` - WebKit bug tracker
- `rdar://12345` - Apple Radar
- `Bug(identifier)` - Other trackers

## Linting

```python
# Lint expectations
warnings = manager.lint(all_tests=test_list)
for w in warnings:
    print(w)
```

The linter checks for:
- Configuration token ordering
- Alphabetical ordering within categories
- Collapsible duplicate entries
- Universal skip detection
- Stale expectations

## Creating Plugins

```python
from webkitexpectationspy.plugins import FormatPlugin

class MyTestPlugin(FormatPlugin):
    @property
    def name(self):
        return 'my-tests'

    @property
    def expectation_map(self):
        return {
            'pass': 0,
            'fail': 1,
            'skip': 4,
            # Add custom expectations
        }

    @property
    def version_tokens(self):
        return {'v1', 'v2', 'v3'}

    @property
    def version_order(self):
        return ['v1', 'v2', 'v3']
```

## License

Modified BSD License. See LICENSE file.
