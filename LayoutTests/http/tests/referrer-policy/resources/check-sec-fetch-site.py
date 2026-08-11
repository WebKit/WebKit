#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

secFetchSite = os.environ.get('HTTP_SEC_FETCH_SITE', 'missing')

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
expected = query.get('expected', [''])[0]

sys.stdout.write('Content-Type: text/html\r\n\r\n')

print('''<script src="/js-test-resources/js-test.js"></script>
<script>
description("Tests the behavior of sec-fetch-site header.");

let secFetchSite = "{}";
let expected = "{}";
shouldBe("secFetchSite", "expected");
if (window.testRunner)
    testRunner.notifyDone();
</script>'''.format(secFetchSite, expected))
