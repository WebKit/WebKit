# Copyright (C) 2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import calendar
import sys

from time import time
from datetime import datetime, tzinfo

if sys.version_info > (3, 2):
    from datetime import timezone

    def fromtimestamp(timestamp=0, tz=None):
        # type: (float, tzinfo|None) -> datetime
        '''Passthrough to `datetime.fromtimestamp` where compatible with `datetime.timezone` (python >=3.2).'''
        fn_kwargs = {'tz': tz} if tz is not None and type(tz) is timezone else {}
        return datetime.fromtimestamp(timestamp, **fn_kwargs)

    def utcfromtimestamp(timestamp=0):
        # type: (float) -> datetime
        '''Converts a `utcfromtimestamp` call to use `fromtimestamp` to avoid deprecation warnings.'''
        return fromtimestamp(timestamp, timezone.utc).replace(tzinfo=None)

else:
    def fromtimestamp(timestamp=0, *_):
        # type: (float, Any) -> datetime
        '''Passthrough to `datetime.fromtimestamp`.'''
        return datetime.fromtimestamp(timestamp)

    def utcfromtimestamp(timestamp=0):
        # type: (float) -> datetime
        '''Passthrough to `datetime.utcfromtimestamp`.'''
        return datetime.utcfromtimestamp(timestamp)


def timestamp(dt):
    # type: (datetime) -> float
    '''Converts a `datetime` object to a timestamp.'''
    return calendar.timegm(dt.timetuple()) + (dt.microsecond / 1000000.0)


def utcnow():
    # type: () -> datetime
    '''Return the current UTC date and time.'''
    return utcfromtimestamp(time())
