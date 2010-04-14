# Required for Python to search this directory for module files

import re

codereview_server_host = "wkrietveld.appspot.com"
codereview_server_regex = "https?://%s/" % re.sub('\.', '\\.', codereview_server_host)
codereview_server_url = "https://%s/" % codereview_server_host
