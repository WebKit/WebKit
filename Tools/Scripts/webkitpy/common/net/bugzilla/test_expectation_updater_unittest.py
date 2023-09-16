# Copyright (C) 2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import io
import json
import unittest
import zipfile

from webkitpy.common.host_mock import MockHost
from webkitpy.common.net.bugzilla.test_expectation_updater import TestExpectationUpdater
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.thirdparty import mock


def _make_zip_filelike(files):
    f = io.BytesIO()
    with zipfile.ZipFile(f, "w", compression=zipfile.ZIP_STORED) as zf:
        for name, content in files.items():
            if name.endswith(".json"):
                content = "ADD_RESULTS(" + json.dumps(content) + ");"
            zf.writestr(name, content)
    f.seek(0)
    return f


_mac_wk2_zip = _make_zip_filelike(
    {
        "full_results.json": {
            "tests": {
                "http": {
                    "tests": {
                        "media": {
                            "hls": {
                                "video-controls-live-stream.html": {
                                    "report": "FLAKY",
                                    "expected": "PASS",
                                    "actual": "TEXT PASS",
                                },
                                "video-duration-accessibility.html": {
                                    "report": "FLAKY",
                                    "expected": "PASS",
                                    "actual": "TEXT PASS",
                                },
                            }
                        }
                    }
                },
                "imported": {
                    "w3c": {
                        "web-platform-tests": {
                            "html": {
                                "browsers": {
                                    "windows": {
                                        "browsing-context.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        }
                                    }
                                }
                            },
                            "fetch": {
                                "api": {
                                    "redirect": {
                                        "redirect-count.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-location.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-count-worker.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-count-cross-origin.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-location-worker.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                    }
                                }
                            },
                        }
                    }
                },
                "media": {
                    "track": {
                        "track-in-band-style.html": {
                            "report": "FLAKY",
                            "expected": "PASS",
                            "actual": "TEXT PASS",
                        }
                    }
                },
            },
            "skipped": 3348,
            "num_regressions": 6,
            "other_crashes": {},
            "interrupted": False,
            "num_missing": 0,
            "layout_tests_dir": "/Volumes/Data/EWS/WebKit/LayoutTests",
            "version": 4,
            "num_passes": 44738,
            "pixel_tests_enabled": False,
            "date": "07:18PM on April 08, 2017",
            "has_pretty_patch": True,
            "fixable": 3357,
            "num_flaky": 3,
            "uses_expectations_file": True,
        },
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-cross-origin-actual.txt": "a",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-worker-actual.txt": "b",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-actual.txt": "c",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-worker-actual.txt": "d",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-actual.txt": "e",
        "imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-actual.txt": "f",
    }
)

_mac_wk1a_zip = _make_zip_filelike(
    {
        "full_results.json": {
            "tests": {
                "http": {
                    "tests": {
                        "loading": {
                            "resourceLoadStatistics": {
                                "non-prevalent-resource-without-user-interaction.html": {
                                    "report": "FLAKY",
                                    "expected": "PASS",
                                    "actual": "TIMEOUT PASS",
                                    "has_stderr": True,
                                }
                            }
                        }
                    }
                },
                "imported": {
                    "w3c": {
                        "web-platform-tests": {
                            "IndexedDB": {
                                "abort-in-initial-upgradeneeded.html": {
                                    "report": "FLAKY",
                                    "expected": "PASS",
                                    "actual": "TEXT PASS",
                                }
                            },
                            "html": {
                                "browsers": {
                                    "windows": {
                                        "browsing-context.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        }
                                    }
                                }
                            },
                            "fetch": {
                                "api": {
                                    "redirect": {
                                        "redirect-count.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-location.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-count-worker.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-count-cross-origin.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-location-worker.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                    }
                                }
                            },
                        },
                        "IndexedDB-private-browsing": {
                            "idbfactory_open9.html": {
                                "report": "FLAKY",
                                "expected": "PASS",
                                "actual": "TIMEOUT PASS",
                                "has_stderr": True,
                            }
                        },
                    },
                    "blink": {
                        "storage": {
                            "indexeddb": {
                                "blob-delete-objectstore-db.html": {
                                    "report": "FLAKY",
                                    "expected": "PASS",
                                    "actual": "TIMEOUT PASS",
                                    "has_stderr": True,
                                }
                            }
                        }
                    },
                },
            },
            "skipped": 3537,
            "num_regressions": 6,
            "other_crashes": {},
            "interrupted": False,
            "num_missing": 0,
            "layout_tests_dir": "/Volumes/Data/EWS/WebKit/LayoutTests",
            "version": 4,
            "num_passes": 44561,
            "pixel_tests_enabled": False,
            "date": "07:18PM on April 08, 2017",
            "has_pretty_patch": True,
            "fixable": 3547,
            "num_flaky": 4,
            "uses_expectations_file": True,
        },
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-cross-origin-actual.txt": "a",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-worker-actual.txt": "b",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-actual.txt": "c",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-worker-actual.txt": "d",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-actual.txt": "e",
        "imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-actual.txt": "f-wk1a",
    }
)

_mac_wk1b_zip = _make_zip_filelike(
    {
        "full_results.json": {
            "tests": {
                "http": {
                    "tests": {
                        "loading": {
                            "resourceLoadStatistics": {
                                "non-prevalent-resource-without-user-interaction.html": {
                                    "report": "FLAKY",
                                    "expected": "PASS",
                                    "actual": "TIMEOUT PASS",
                                    "has_stderr": True,
                                }
                            }
                        }
                    }
                },
                "imported": {
                    "w3c": {
                        "web-platform-tests": {
                            "IndexedDB": {
                                "abort-in-initial-upgradeneeded.html": {
                                    "report": "FLAKY",
                                    "expected": "PASS",
                                    "actual": "TEXT PASS",
                                }
                            },
                            "html": {
                                "browsers": {
                                    "windows": {
                                        "browsing-context.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        }
                                    }
                                }
                            },
                            "fetch": {
                                "api": {
                                    "redirect": {
                                        "redirect-count.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-location.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-count-worker.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-count-cross-origin.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                        "redirect-location-worker.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        },
                                    }
                                }
                            },
                        },
                        "IndexedDB-private-browsing": {
                            "idbfactory_open9.html": {
                                "report": "FLAKY",
                                "expected": "PASS",
                                "actual": "TIMEOUT PASS",
                                "has_stderr": True,
                            }
                        },
                    },
                    "blink": {
                        "storage": {
                            "indexeddb": {
                                "blob-delete-objectstore-db.html": {
                                    "report": "FLAKY",
                                    "expected": "PASS",
                                    "actual": "TIMEOUT PASS",
                                    "has_stderr": True,
                                }
                            }
                        }
                    },
                },
            },
            "skipped": 3537,
            "num_regressions": 6,
            "other_crashes": {},
            "interrupted": False,
            "num_missing": 0,
            "layout_tests_dir": "/Volumes/Data/EWS/WebKit/LayoutTests",
            "version": 4,
            "num_passes": 44561,
            "pixel_tests_enabled": False,
            "date": "07:18PM on April 08, 2017",
            "has_pretty_patch": True,
            "fixable": 3547,
            "num_flaky": 4,
            "uses_expectations_file": True,
        },
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-cross-origin-actual.txt": "a",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-worker-actual.txt": "b",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-actual.txt": "c",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-worker-actual.txt": "d",
        "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-actual.txt": "e",
        "imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-actual.txt": "f-wk1b",
    }
)

_ios_zip = _make_zip_filelike(
    {
        "full_results.json": {
            "tests": {
                "imported": {
                    "w3c": {
                        "web-platform-tests": {
                            "url": {
                                "interfaces.html": {
                                    "report": "REGRESSION",
                                    "expected": "PASS",
                                    "actual": "TEXT",
                                }
                            },
                            "html": {
                                "browsers": {
                                    "windows": {
                                        "browsing-context.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        }
                                    },
                                    "the-window-object": {
                                        "apis-for-creating-and-navigating-browsing-contexts-by-name": {
                                            "open-features-tokenization-001.html": {
                                                "report": "REGRESSION",
                                                "expected": "PASS",
                                                "actual": "TEXT",
                                            }
                                        }
                                    },
                                }
                            },
                            "dom": {
                                "events": {
                                    "EventTarget-dispatchEvent.html": {
                                        "report": "REGRESSION",
                                        "expected": "PASS",
                                        "actual": "TEXT",
                                    }
                                }
                            },
                        }
                    }
                },
                "animations": {
                    "trigger-container-scroll-empty.html": {
                        "report": "FLAKY",
                        "expected": "PASS",
                        "actual": "TEXT PASS",
                    }
                },
            },
            "skipped": 9881,
            "num_regressions": 4,
            "other_crashes": {},
            "interrupted": False,
            "num_missing": 0,
            "layout_tests_dir": "/Volumes/Data/EWS/WebKit/LayoutTests",
            "version": 4,
            "num_passes": 38225,
            "pixel_tests_enabled": False,
            "date": "07:33PM on April 08, 2017",
            "has_pretty_patch": True,
            "fixable": 48110,
            "num_flaky": 1,
            "uses_expectations_file": True,
        },
        "imported/w3c/web-platform-tests/dom/events/EventTarget-dispatchEvent-actual.txt": "g",
        "imported/w3c/web-platform-tests/html/browsers/the-window-object/apis-for-creating-and-navigating-browsing-contexts-by-name/open-features-tokenization-001-actual.txt": "h",
        "imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-actual.txt": "i",
        "imported/w3c/web-platform-tests/url/interfaces-actual.txt": "j",
    }
)

_win_zip = _make_zip_filelike(
    {
        "full_results.json": {
            "tests": {
                "imported": {
                    "w3c": {
                        "web-platform-tests": {
                            "url": {
                                "interfaces.html": {
                                    "report": "REGRESSION",
                                    "expected": "PASS",
                                    "actual": "TEXT",
                                }
                            },
                            "html": {
                                "browsers": {
                                    "windows": {
                                        "browsing-context.html": {
                                            "report": "REGRESSION",
                                            "expected": "PASS",
                                            "actual": "TEXT",
                                        }
                                    },
                                    "the-window-object": {
                                        "apis-for-creating-and-navigating-browsing-contexts-by-name": {
                                            "open-features-tokenization-001.html": {
                                                "report": "REGRESSION",
                                                "expected": "PASS",
                                                "actual": "TEXT",
                                            }
                                        }
                                    },
                                }
                            },
                            "dom": {
                                "events": {
                                    "EventTarget-dispatchEvent.html": {
                                        "report": "REGRESSION",
                                        "expected": "PASS",
                                        "actual": "TEXT",
                                    }
                                }
                            },
                        }
                    }
                },
                "animations": {
                    "trigger-container-scroll-empty.html": {
                        "report": "FLAKY",
                        "expected": "PASS",
                        "actual": "TEXT PASS",
                    }
                },
            },
            "skipped": 9881,
            "num_regressions": 4,
            "other_crashes": {},
            "interrupted": False,
            "num_missing": 0,
            "layout_tests_dir": "/Volumes/Data/EWS/WebKit/LayoutTests",
            "version": 4,
            "num_passes": 38225,
            "pixel_tests_enabled": False,
            "date": "07:33PM on April 08, 2017",
            "has_pretty_patch": True,
            "fixable": 48110,
            "num_flaky": 1,
            "uses_expectations_file": True,
        },
        "imported/w3c/web-platform-tests/dom/events/EventTarget-dispatchEvent-actual.txt": "g",
        "imported/w3c/web-platform-tests/html/browsers/the-window-object/apis-for-creating-and-navigating-browsing-contexts-by-name/open-features-tokenization-001-actual.txt": "h",
        "imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-actual.txt": "i",
        "imported/w3c/web-platform-tests/url/interfaces-actual.txt": "j",
    }
)


class MockRequestsGet:
    def __init__(self, url):
        self._urls_data = {
            "https://ews-build.webkit.org/results/mac-wk1/r12345.zip": _mac_wk1a_zip.getvalue(),
            "https://ews-build.webkit.org/results/mac-debug-wk1/r12345.zip": _mac_wk1b_zip.getvalue(),
            "https://ews-build.webkit.org/results/mac-wk2/r12345.zip": _mac_wk2_zip.getvalue(),
            "https://ews-build.webkit.org/results/ios-wk2/r12345.zip": _ios_zip.getvalue(),
            "https://ews-build.webkit.org/results/win/r12345.zip": _win_zip.getvalue(),
        }
        self._url = url
        if url not in self._urls_data:
            msg = 'url {} not mocked'.format(url)
            raise ValueError(msg)

    @property
    def content(self):
        c = self._urls_data[self._url]
        assert isinstance(c, bytes)
        return c

    @property
    def text(self):
        return self.content.decode("utf-8")

    def raise_for_status(self):
        pass

    def json(self):
        return json.loads(self.content)


class TestExpectationUpdaterTest(unittest.TestCase):
    def _exists(self, host, filename):
        return host.filesystem.exists("/mock-checkout/LayoutTests/" + filename)

    def _is_matching(self, host, filename, content):
        return host.filesystem.read_text_file("/mock-checkout/LayoutTests/" + filename) == content

    def test_update_test_expectations(self):
        host = MockHost()
        host.executive = MockExecutive2(exception=OSError())
        host.filesystem = MockFileSystem(files={
            '/mock-checkout/LayoutTests/platform/mac-wk1/imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-expected.txt': 'e-wk1',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/dom/events/EventTarget-dispatchEvent-expected.txt': "g",
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/html/browsers/the-window-object/apis-for-creating-and-navigating-browsing-contexts-by-name/open-features-tokenization-001-expected.txt': "h",
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-expected.txt': "i",
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/url/interfaces-expected.txt': "j-mac-wk2"})

        ews_results = {
            "mac-wk1": [
                "https://ews-build.webkit.org/results/mac-wk1/r12345.zip",
                "https://ews-build.webkit.org/results/mac-debug-wk1/r12345.zip",
            ],
            "mac-wk2": ["https://ews-build.webkit.org/results/mac-wk2/r12345.zip"],
            "ios-wk2": ["https://ews-build.webkit.org/results/ios-wk2/r12345.zip"],
            "win": ["https://ews-build.webkit.org/results/win/r12345.zip"],
        }

        with mock.patch('webkitpy.common.net.bugzilla.test_expectation_updater.lookup_ews_results_from_bugzilla', mock.Mock(return_value=ews_results)), mock.patch('requests.get', MockRequestsGet):
            updater = TestExpectationUpdater(host, "123456", True, False, None)
            updater.do_update()
            # mac-wk2 expectation
            self.assertTrue(self._is_matching(host, "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-cross-origin-expected.txt", "a"))
            # no need to add mac-wk1 specific expectation
            self.assertFalse(self._exists(host, "platform/mac-wk1/imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-cross-origin-expected.txt"))
            # mac-wk1/ios-simulator-wk2 specific expectation
            self.assertTrue(self._is_matching(host, "platform/mac-wk1/imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-expected.txt", "f-wk1b"))
            self.assertTrue(self._is_matching(host, "platform/ios-wk2/imported/w3c/web-platform-tests/url/interfaces-expected.txt", "j"))
            # removal of mac-wk1 expectation since no longer different
            self.assertFalse(self._exists(host, "platform/mac-wk1/imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-expected.txt"))
            # windows specific expectation
            self.assertTrue(self._is_matching(host, "platform/win/imported/w3c/web-platform-tests/url/interfaces-expected.txt", "j"))
