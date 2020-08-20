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
