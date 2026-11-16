#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

referer = os.environ.get('HTTP_REFERER', '')

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
policy = query.get('policy', [''])[0]
expected = query.get('expected', [''])[0]

sys.stdout.write('Content-Type: text/html\r\n\r\n')

print('''<script src="/js-test-resources/js-test.js"></script>
<script>
description("Tests the behavior of {} referrer policy.");

let referrer = "{}";
let expected = "{}";
shouldBe("referrer", "expected");
if (window.testRunner)
    testRunner.notifyDone();
</script>'''.format(policy, referer, expected))