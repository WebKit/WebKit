# Copyright (C) 2019 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import atexit
import json
import os
import requests
import unittest

from flask import Flask
from flask.wrappers import Response
from selenium import webdriver
from selenium.common.exceptions import WebDriverException
from resultsdbpy.flask_support.flask_test_context import FlaskTestContext


class FlaskRequestsResponse(Response):
    @property
    def text(self):
        return self.data.decode('utf-8')

    @property
    def content(self):
        return self.data

    def json(self):
        return json.loads(self.text)


class FlaskTestCase(unittest.TestCase):
    URL = f'http://localhost:{FlaskTestContext.PORT}'

    _driver = None
    _cached_driver = False
    _printed_webserver_warning = False

    @classmethod
    def driver(cls):
        if cls._cached_driver:
            return cls._driver

        try:
            result = webdriver.Safari()
            result.get('about:blank')
            atexit.register(result.close)
            cls._driver = result
        except WebDriverException as e:
            print(e.msg)

        except ImportError:
            print('Selenium is not installed, run \'pip install selenium\'')

        cls._cached_driver = True
        return cls._driver

    @classmethod
    def setup_webserver(cls, app, **kwargs):
        raise NotImplementedError()

    @classmethod
    def combine(cls, *args):
        def decorator(func):
            for elm in reversed(args):
                func = elm(func)
            return func

        return decorator

    @classmethod
    def run_with_real_webserver(cls):
        def decorator(method):
            def real_method(val, method=method, **kwargs):
                with FlaskTestContext(lambda app: val.setup_webserver(app, **kwargs)):
                    return method(val, client=requests, **kwargs)
            real_method.__name__ = method.__name__
            return real_method

        return cls.combine(
            unittest.skipIf(not int(os.environ.get('web_server', '0')), 'WebServer tests disabled'),
            decorator,
        )

    @classmethod
    def run_with_mock_webserver(cls):
        def decorator(method):
            def real_method(val, method=method, **kwargs):
                app = Flask('testing')
                app.response_class = FlaskRequestsResponse
                app.config['TESTING'] = True
                val.setup_webserver(app, **kwargs)
                app.add_url_rule('/__health', 'health', lambda: 'ok', methods=('GET',))
                return method(val, client=app.test_client(), **kwargs)

            real_method.__name__ = method.__name__
            return real_method

        return decorator

    @classmethod
    def run_with_webserver(cls):
        if int(os.environ.get('web_server', '0') and int(os.environ.get('slow_tests', '0'))):
            if not cls._printed_webserver_warning:
                print('Using real web server, requests routed through requests library')
            cls._printed_webserver_warning = True
            return cls.run_with_real_webserver()
        if not cls._printed_webserver_warning:
            print('Using mock web server, requests routed through flask testing framework')
        cls._printed_webserver_warning = True
        return cls.run_with_mock_webserver()

    @classmethod
    def run_with_selenium(cls):

        def decorator(method):
            def real_method(val, method=method, **kwargs):
                return method(val, driver=cls.driver(), **kwargs)
            real_method.__name__ = method.__name__
            return real_method

        return cls.combine(
            cls.run_with_real_webserver(),
            unittest.skipIf(not int(os.environ.get('selenium', '0')), 'Selenium tests disabled'),
            unittest.skipIf(not cls.driver(), 'Selenium not available for testing'),
            decorator,
        )
