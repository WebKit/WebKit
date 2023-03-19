#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Link: <../resources/dummy.css>; rel=preload; as=style; fetchpriority=low;\r\n'
    'Link: <../resources/dummy.css?1>; rel=preload; as=style;\r\n'
    'Link: <../resources/dummy.css?2>; rel=preload; as=style; fetchpriority=high;\r\n'
    'Link: <../resources/dummy.js>; rel=preload; as=script; fetchpriority=low;\r\n'
    'Link: <../resources/dummy.js?1>; rel=preload; as=script;\r\n'
    'Link: <../resources/dummy.js?2>; rel=preload; as=script; fetchpriority=high;\r\n'
    'Link: <../resources/dummy.txt>; rel=preload; as=fetch; fetchpriority=low;\r\n'
    'Link: <../resources/dummy.txt?1>; rel=preload; as=fetch;\r\n'
    'Link: <../resources/dummy.txt?2>; rel=preload; as=fetch; fetchpriority=high;\r\n'
    'Link: <../resources/square.png>; rel=preload; as=image; fetchpriority=low;\r\n'
    'Link: <../resources/square.png?1>; rel=preload; as=image;\r\n'
    'Link: <../resources/square.png?2>; rel=preload; as=image; fetchpriority=high;\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<meta charset="utf-8">
<script src=../resources/testharness.js></script>
<script src=../resources/testharnessreport.js></script>
<script src="http://127.0.0.1:8000/resources/slow-script.pl?delay=100"></script>
<script src="http://127.0.0.1:8000/resources/checkResourcePriority.js"></script>

<script>
const priority_tests = [
  {
    url: new URL('../resources/dummy.css', location), expected_priority: "ResourceLoadPriorityMedium",
    description: 'low fetchpriority on <link rel=preload as=style> must be fetched with medium resource load priority'
  },
  {
    url: new URL('../resources/dummy.css?1', location), expected_priority: "ResourceLoadPriorityHigh",
    description: 'missing fetchpriority on <link rel=preload as=style> must have no effect on resource load priority'
  },
  {
    url: new URL('../resources/dummy.css?2', location), expected_priority: "ResourceLoadPriorityVeryHigh",
    description: 'high fetchpriority on <link rel=preload as=style> must be fetched with very high resource load priority'
  },
  {
    url: new URL('../resources/dummy.css', location), expected_priority: "ResourceLoadPriorityMedium",
    description: 'low fetchpriority on <link rel=preload as=script> must be fetched with medium resource load priority'
  },
  {
    url: new URL('../resources/dummy.css?1', location), expected_priority: "ResourceLoadPriorityHigh",
    description: 'missing fetchpriority on <link rel=preload as=script> must have no effect on resource load priority'
  },
  {
    url: new URL('../resources/dummy.css?2', location), expected_priority: "ResourceLoadPriorityVeryHigh",
    description: 'high fetchpriority on <link rel=preload as=script> must be fetched with very high resource load priority'
  },
  {
    url: new URL('../resources/dummy.css', location), expected_priority: "ResourceLoadPriorityMedium",
    description: 'low fetchpriority on <link rel=preload as=fetch> must be fetched with medium resource load priority'
  },
  {
    url: new URL('../resources/dummy.css?1', location), expected_priority: "ResourceLoadPriorityHigh",
    description: 'missing fetchpriority on <link rel=preload as=fetch> must have no effect on resource load priority'
  },
  {
    url: new URL('../resources/dummy.css?2', location), expected_priority: "ResourceLoadPriorityVeryHigh",
    description: 'high fetchpriority on <link rel=preload as=fetch> must be fetched with very high resource load priority'
  },
  {
    url: new URL('../resources/square.png', location), expected_priority: "ResourceLoadPriorityVeryLow",
    description: 'low fetchpriority on <link rel=preload as=image> must be fetched with very low resource load priority'
  },
  {
    url: new URL('../resources/square.png?1', location), expected_priority: "ResourceLoadPriorityLow",
    description: 'missing fetchpriority on <link rel=preload as=image> must have no effect on resource load priority'
  },
  {
    url: new URL('../resources/square.png?2', location), expected_priority: "ResourceLoadPriorityMedium",
    description: 'high fetchpriority on <link rel=preload as=image> must be fetched with medium resource load priority'
  }
];
</script>

<script>
  promise_test(async (t) => {
    await new Promise(resolve => {
      addEventListener('DOMContentLoaded', resolve);
    });

    const base_msg = ' was fetched by the preload scanner';
    for (const test of priority_tests) {
      assert_true(internals.isPreloaded(test.url), test.url + base_msg);
    }
  }, 'all preloads must be fetched by the preload scanner');

 // Setup the tests described by |priority_tests|.
  for (const test of priority_tests) {
    async_test(t => {
      checkResourcePriority(test.url, test.expected_priority, test.description);
      t.done();
    });
  }
</script>''')
