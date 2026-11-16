# Licensed to the Software Freedom Conservancy (SFC) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The SFC licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

from .common.action_chains import ActionChains  # noqa
from .common.desired_capabilities import DesiredCapabilities  # noqa
from .common.keys import Keys  # noqa
from .common.proxy import Proxy  # noqa
from .remote.webdriver import WebDriver as Remote  # noqa
from .safari.options import Options as SafariOptions
from .safari.service import Service as SafariService  # noqa
from .safari.webdriver import WebDriver as Safari  # noqa
from .webkitgtk.options import Options as WebKitGTKOptions  # noqa
from .webkitgtk.service import Service as WebKitGTKService  # noqa
from .webkitgtk.webdriver import WebDriver as WebKitGTK  # noqa
from .wpewebkit.options import Options as WPEWebKitOptions  # noqa
from .wpewebkit.service import Service as WPEWebKitService  # noqa
from .wpewebkit.webdriver import WebDriver as WPEWebKit  # noqa

__version__ = "4.36.0.202508121825"

# We need an explicit __all__ because the above won't otherwise be exported.
__all__ = [
    "ActionChains",
    "DesiredCapabilities",
    "Keys",
    "Proxy",
    "Remote",
    "Safari",
    "SafariOptions",
    "SafariService",
    "WPEWebKit",
    "WPEWebKitOptions",
    "WPEWebKitService",
    "WebKitGTK",
    "WebKitGTKOptions",
    "WebKitGTKService",
]
