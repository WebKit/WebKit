# webkitcorepy

Provides a number of utilities intended to support intermediate to advanced Python programming.

## Requirements

The mock and requests libraries.
 
## Usage

Version representation
```
from webkitcorepy import Version
version = Version(1, 2, 3)
```

Unicode stream management across Python 2 and 3
```
from webkitcorepy import BytesIO, StringIO, UnicodeIO, unicode
```

Encoding and decoding byte strings and unicode strings
```
from webkitcorepy import string_utils

string_utils.encode(...)
string_utils.decode(...)
```

Automatically install libraries on import.
```
from webkitcorepy import AutoInstall
AutoInstall.register(Package('requests', Version(2, 24)))
import requests
```

Mocking basic time and sleep  calls
```
import time
from webkitcorepy import mocks

with mocks.Time:
    stamp = time.time()
    time.sleep(5)
```
Capturing stdout, stderr and logging output for testing
```
capturer = OutputCapture()
with capturer:
    print('data\n')
assert capturer.stdout.getvalue() == 'data\n'
```
Capturing stdout, stderr and logging output for testing
```
capturer = OutputCapture()
with capturer:
    print('data\n')
assert capturer.stdout.getvalue() == 'data\n'
```

Timeout context:
```
import time

from webkitcorepy import Timeout

with Timeout(5, handler=RuntimeError('Exceeded 5 second timeout')):
    time.sleep(4)
```

subprocess.run replacement:
```
import sys

from webkitcorepy import run

result = run([sys.executable, '-c', 'print("message")'], capture_output=True, encoding='utf-8')
```

Mocking of subprocess commands:
```
from webkitcorepy import mocks, run

with mocks.Subprocess(
    'ls', completion=mocks.ProcessCompletion(returncode=0, stdout='file1.txt\nfile2.txt\n'),
):
    result = run(['ls'], capture_output=True, encoding='utf-8')
    assert result.returncode == 0
    assert result.stdout == 'file1.txt\nfile2.txt\n'
```
The mocking system for subprocess also supports other subprocess APIs based on Popen:
```
with mocks.Subprocess(
    'ls', completion=mocks.ProcessCompletion(returncode=0, stdout='file1.txt\nfile2.txt\n'),
):
    assert subprocess.check_output(['ls']) == b'file1.txt\nfile2.txt\n'
    assert subprocess.check_call(['ls']) == 0
```
For writing integration tests, the mocking system for subprocess supports mocking multiple process calls at the same time:
```
with mocks.Subprocess(
    mocks.Subprocess.CommandRoute('command-a', 'argument', completion=mocks.ProcessCompletion(returncode=0)),
    mocks.Subprocess.CommandRoute('command-b', completion=mocks.ProcessCompletion(returncode=-1)),
):
    result = run(['command-a', 'argument'])
    assert result.returncode == 0

    result = run(['command-b'])
    assert result.returncode == -1
```
