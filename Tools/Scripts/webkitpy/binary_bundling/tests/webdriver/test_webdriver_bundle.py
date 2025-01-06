#!/usr/bin/env python3
#
# Copyright (C) 2024 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
from selenium import webdriver
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.common.by import By
from selenium.common.exceptions import TimeoutException
from PIL import Image
import argparse
import numpy as np
import os
import io
import time
import sys

MYDIR = os.path.dirname(os.path.realpath(__file__))


def callfunc(obj, method_name, *args, **kwargs):
    method = getattr(obj, method_name, None)
    if callable(method):
        return method(*args, **kwargs)
    else:
        print(f'No such method: {method_name}')
        return False


class WebDriverTests():

    def __init__(self, browser_path, webdriver_path, platform_name, number_retries):
        self.started = False
        if platform_name == 'gtk':
            from selenium.webdriver import WebKitGTKOptions as WebKitOptions
            from selenium.webdriver.webkitgtk.service import Service
            from selenium.webdriver import WebKitGTK as WebKitDriver
        elif platform_name == 'wpe':
            from selenium.webdriver import WPEWebKitOptions as WebKitOptions
            from selenium.webdriver.wpewebkit.service import Service
            from selenium.webdriver import WPEWebKit as WebKitDriver
        else:
            raise NotImplementedError(f'Unknown platform {platform_name}')
        options = WebKitOptions()
        options.binary_location = browser_path
        options.add_argument('--automation')
        if platform_name == 'wpe':
            options.add_argument('--headless')
        service = Service(executable_path=webdriver_path, service_args=['--host=127.0.0.1'])
        self.driver = WebKitDriver(options=options, service=service)
        self.driver.maximize_window()
        self.number_retries = max(number_retries, 1)
        self.started = True

    def __del__(self):
        if self.started:
            self.driver.quit()

    def test_dynamic_rendering(self, test_url):
        self.driver.get(test_url)

        try:
            WebDriverWait(self.driver, 10).until(EC.text_to_be_present_in_element((By.ID, 'dynamic-content'), 'Dynamic content loaded!'))
            return True
        except Exception as e:
            print(f'Dynamic content rendering test failed: {e}')
            return False

    def test_video_playback(self, test_url):
        self.driver.get(test_url)

        try:
            WebDriverWait(self.driver, 10).until(EC.presence_of_element_located((By.TAG_NAME, 'video')))
            WebDriverWait(self.driver, 10).until(lambda d: self.driver.execute_script('return isVideoReady();'))
            # Check is not playing at startup
            if self.driver.execute_script('return isVideoPlaying();'):
                raise RuntimeError('Video should not play at startup')
            # Request to play and check is playing
            self.driver.execute_script('startVideoPlayback();')
            time.sleep(5)
            if not self.driver.execute_script('return isVideoPlaying();'):
                raise RuntimeError('Video should play when requested')
            # video length is 10, wait more than enough (5+10) to finish and check if finished
            time.sleep(10)
            if self.driver.execute_script('return isVideoPlaying();'):
                raise RuntimeError('Video should have stopped playing after 12 seconds')
            return True
        except Exception as e:
            print(f'Video playback test failed: {e}')
            return False

    def test_webgl_support(self, test_url):
        subtests_failed = 0

        self.driver.get(test_url)

        webgl_status = WebDriverWait(self.driver, 10).until(EC.presence_of_element_located((By.ID, 'webgl-status')))

        if 'WebGL 1.0 is supported.' not in webgl_status.text:
            subtests_failed += 1
            print('WebGL 1.0 not supported.')

        # Check WebGL 1.0 extensions
        webgl_extensions = self.driver.find_elements(By.CSS_SELECTOR, '#webgl-extensions li')
        if len(webgl_extensions) < 10:
            subtests_failed += 1
            print(f'WebGL 1.0 has fewer than 10 extensions (found {len(webgl_extensions)}).')

        # Wait for WebGL 2.0 status to appear
        webgl2_status = WebDriverWait(self.driver, 10).until(EC.presence_of_element_located((By.ID, 'webgl2-status')))

        # Check if WebGL 2.0 is supported
        if 'WebGL 2.0 is supported.' not in webgl2_status.text:
            subtests_failed += 1
            print('WebGL 2.0 not supported.')

        # Check WebGL 2.0 extensions
        webgl2_extensions = self.driver.find_elements(By.CSS_SELECTOR, '#webgl2-extensions li')
        if len(webgl2_extensions) < 10:
            subtests_failed += 1
            print(f'WebGL 2.0 has fewer than 10 extensions (found {len(webgl2_extensions)}).')

        return subtests_failed == 0

    def test_css_animations(self, test_url):
        self.driver.get(test_url)

        try:
            animated_element = WebDriverWait(self.driver, 10).until(
                EC.presence_of_element_located((By.ID, 'cube'))
            )

            initial_transform = self.driver.execute_script("return window.getComputedStyle(arguments[0]).getPropertyValue('transform')", animated_element)

            WebDriverWait(self.driver, 10).until(lambda d: self.driver.execute_script("return window.getComputedStyle(arguments[0]).getPropertyValue('transform')", animated_element) != initial_transform)
            return True
        except Exception as e:
            print(f"3D CSS animation test failed: {e}")
            return False

    def test_webgl_rainbow(self, test_url):

        def get_average_color(image):
            """Get the average color of an image."""
            image = image.convert('RGB')
            np_image = np.array(image)
            avg_color = np.mean(np_image, axis=(0, 1)) / 255
            return avg_color

        def test_color_filling(driver, color):
            """Test if the canvas is filled with the specified color."""
            driver.get(test_url)

            # Fill the canvas with the color
            driver.execute_script(f"fillWindowWithColor('{color}');")

            # Wait for the color to be applied
            time.sleep(1)

            # Capture and analyze the screenshot
            screenshot = driver.get_screenshot_as_png()
            screenshot = Image.open(io.BytesIO(screenshot))
            avg_color = get_average_color(screenshot)

            # Convert color from hex to RGB
            hex_color = [int(color[i:i + 2], 16) / 255 for i in (1, 3, 5)]

            # Check if the average color of the screenshot matches the expected color
            tolerance = 0.1
            color_match = all(abs(avg_color[i] - hex_color[i]) < tolerance for i in range(3))

            if color_match:
                return True
            print(f'test_webgl_rainbow subtest failed: The canvas color does not match {color}.')
            return False

        subtest_passed = 0
        rainbow_colors_hex = ['#FF0000', '#FF3300', '#FF6600', '#FF9900', '#FFFF00', '#CCFF00', '#00FF00',
                              '#00FFCC', '#00FFFF', '#00CCFF', '#0000FF', '#6600FF', '#FF00FF', '#FF00CC']

        for color in rainbow_colors_hex:
            if test_color_filling(self.driver, color):
                subtest_passed += 1

        return subtest_passed == len(rainbow_colors_hex)

    def test_ssl_errors(self, url):
        try:
            self.driver.get('about:blank')
            self.driver.get(url)
            WebDriverWait(self.driver, 5).until(lambda d: d.execute_script("return window.location.href != 'about:blank'"))
            print(f'FAIL: Browser loaded page with SSL errors: {url}')
            return False
        except TimeoutException:
            # good, the browser failed to load the site, double check it just loaded blank
            return self.driver.execute_script("return window.location.href == 'about:blank'")
        except Exception as e:
            print(f'An error occurred: {e}')
            return False

    def test_wasm_doom_load(self, url):
        try:
            self.driver.get(url)
            # Wait for the page to load
            WebDriverWait(self.driver, 10).until(lambda d: d.execute_script(f"return window.location.href.startsWith('{url}')"))
            WebDriverWait(self.driver, 10).until(lambda d: d.execute_script('return document.readyState') == 'complete')
            time.sleep(5)
            one_doom_fps = int(self.driver.find_element(By.ID, 'fps_stats').text)
            one_doom_total_frames = int(self.driver.find_element(By.ID, 'drawframes_stats').text)
            one_doom_total_calls = int(self.driver.find_element(By.ID, 'getms_stats').text)
            time.sleep(5)
            two_doom_fps = int(self.driver.find_element(By.ID, 'fps_stats').text)
            two_doom_total_frames = int(self.driver.find_element(By.ID, 'drawframes_stats').text)
            two_doom_total_calls = int(self.driver.find_element(By.ID, 'getms_stats').text)
            if one_doom_fps < 5 or one_doom_fps > 60:
                raise RuntimeError(f'Unexpected fps value for one_doom: {one_doom_fps}')
            if two_doom_fps < 5 or two_doom_fps > 60:
                raise RuntimeError(f'Unexpected fps value for two_doom: {two_doom_fps}')
            if one_doom_total_frames + 25 >= two_doom_total_frames:
                raise RuntimeError(f"Total frames didn't advanced as expected: {one_doom_total_frames} vs {two_doom_total_frames}")
            if one_doom_total_calls + 250 >= two_doom_total_calls:
                raise RuntimeError(f"Total calls didn't advanced as expected: {one_doom_total_calls} vs {two_doom_total_calls}")
            return True
        except Exception as e:
            print(f'An error occurred: {e}')
            return False

    def do_test(self, test_name, test_url):
        for i in range(1, self.number_retries + 1):
            if callfunc(self, test_name, test_url):
                print(f'PASS [try:{i}]: {test_name} {test_url}')
                return 0
            else:
                print(f'FAIL [try:{i}]: {test_name} {test_url}')
        print(f'Number of maximum retries reached: {self.number_retries}')
        print(f'FAIL [ALL RETRIES]: {test_name} {test_url}')
        return 1

    def do_local_test(self, test_name):
        html_test_path = os.path.join(MYDIR, test_name + '.html')
        if not os.path.isfile(html_test_path):
            print(f'FAIL: can not found file: {html_test_path}')
            return 1
        html_test_url = f'file://{html_test_path}'
        return self.do_test(test_name, html_test_url)


def run_all_tests(bundle_dir, platform_name, number_retries):
    assert (platform_name in ['gtk', 'wpe'])
    if not os.path.isdir(bundle_dir):
        raise ValueError(f'{bundle_dir} is not a valid directory')
    browser_path = os.path.join(bundle_dir, 'MiniBrowser')
    webdriver_name = 'WebKitWebDriver' if platform_name == 'gtk' else 'WPEWebDriver'
    webdriver_path = os.path.join(bundle_dir, webdriver_name)
    for binary_path in [browser_path, webdriver_path]:
        if not (os.path.isfile(binary_path) and os.access(binary_path, os.X_OK)):
            raise ValueError(f'Can not find an executable at: {binary_path}')
    tester = WebDriverTests(browser_path, webdriver_path, platform_name, number_retries)
    total_tests_failed = 0
    for test in ['test_dynamic_rendering', 'test_video_playback', 'test_css_animations', 'test_webgl_support', 'test_webgl_rainbow']:
        total_tests_failed += tester.do_local_test(test)
    for bad_ssl_url in ['https://expired.badssl.com/', 'https://wrong.host.badssl.com/', 'https://self-signed.badssl.com/', 'https://untrusted-root.badssl.com/']:
        total_tests_failed += tester.do_test('test_ssl_errors', bad_ssl_url)
    total_tests_failed += tester.do_test('test_wasm_doom_load', 'https://people.igalia.com/clopez/wkbug/wasm/doom/index.html')
    if total_tests_failed == 0:
        print('PASSED all tests: OK')
    elif total_tests_failed == 1:
        print(f'FAILED 1 test that was retried {number_retries} times')
    else:
        print(f'FAILED a total of {total_tests_failed} tests each one retried {number_retries} times')
    return total_tests_failed

def main():
    parser = argparse.ArgumentParser('usage: %prog [options]')
    parser.add_argument('--platform', dest='platform', choices=['gtk', 'wpe'], required=True,
                        help='The WebKit port to test the bundle')
    parser.add_argument('--number-retries-test', dest='number_retries', default=3, type=int,
                        help='The number of retries per test before marking it as a failure.')
    parser.add_argument('bundle_dir', help='Directory path where the bundle is uncompressed.')
    options = parser.parse_args()
    return run_all_tests(options.bundle_dir, options.platform, options.number_retries)


if __name__ == '__main__':
    sys.exit(main())
