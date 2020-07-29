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
