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

import pytest

from selenium.common.exceptions import NoSuchWindowException
from selenium.common.exceptions import WebDriverException
from selenium.webdriver.common.by import By
from selenium.webdriver.common.window import WindowTypes
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait


@pytest.fixture(autouse=True)
def close_windows(driver):
    main_windows_handle = driver.current_window_handle
    yield
    try:
        from urllib import request as url_request
    except ImportError:
        import urllib2 as url_request
    URLError = url_request.URLError

    try:
        window_handles = driver.window_handles
    except URLError:
        return
    for handle in window_handles:
        if handle != main_windows_handle:
            driver.switch_to.window(handle)
            driver.close()
    driver.switch_to.window(main_windows_handle)


def testShouldSwitchFocusToANewWindowWhenItIsOpenedAndNotStopFutureOperations(driver, pages):
    pages.load("xhtmlTest.html")
    current = driver.current_window_handle

    driver.find_element(By.LINK_TEXT, "Open new window").click()
    assert driver.title == "XHTML Test Page"
    handles = driver.window_handles
    handles.remove(current)
    driver.switch_to.window(handles[0])
    assert driver.title == "We Arrive Here"

    pages.load("iframes.html")
    handle = driver.current_window_handle
    driver.find_element(By.ID, "iframe_page_heading")
    driver.switch_to.frame(driver.find_element(By.ID, "iframe1"))
    assert driver.current_window_handle == handle


def testCanSwitchToWindowByName(driver, pages):
    pages.load("xhtmlTest.html")
    handles = driver.window_handles
    driver.find_element(By.LINK_TEXT, "Open new window").click()
    WebDriverWait(driver, 3).until(EC.new_window_is_opened(handles))
    driver.switch_to.window("result")
    assert driver.title == "We Arrive Here"


def testShouldThrowNoSuchWindowException(driver, pages):
    pages.load("xhtmlTest.html")
    with pytest.raises(NoSuchWindowException):
        driver.switch_to.window("invalid name")


@pytest.mark.xfail_safari
def testShouldThrowNoSuchWindowExceptionOnAnAttemptToGetItsHandle(driver, pages):
    pages.load("xhtmlTest.html")
    current = driver.current_window_handle
    handles = driver.window_handles
    driver.find_element(By.LINK_TEXT, "Open new window").click()
    WebDriverWait(driver, 3).until(EC.new_window_is_opened(handles))
    handles = driver.window_handles
    handles.remove(current)
    driver.switch_to.window(handles[0])
    driver.close()

    with pytest.raises(NoSuchWindowException):
        driver.current_window_handle


@pytest.mark.xfail_ie
def testShouldThrowNoSuchWindowExceptionOnAnyOperationIfAWindowIsClosed(driver, pages):
    pages.load("xhtmlTest.html")
    current = driver.current_window_handle
    handles = driver.window_handles
    driver.find_element(By.LINK_TEXT, "Open new window").click()
    WebDriverWait(driver, 3).until(EC.new_window_is_opened(handles))
    handles = driver.window_handles
    handles.remove(current)
    driver.switch_to.window(handles[0])
    driver.close()

    with pytest.raises(NoSuchWindowException):
        driver.title

    with pytest.raises(NoSuchWindowException):
        driver.find_element(By.TAG_NAME, "body")


@pytest.mark.xfail_ie
@pytest.mark.xfail_safari
def testShouldThrowNoSuchWindowExceptionOnAnyElementOperationIfAWindowIsClosed(driver, pages):
    pages.load("xhtmlTest.html")
    current = driver.current_window_handle
    handles = driver.window_handles
    driver.find_element(By.LINK_TEXT, "Open new window").click()
    WebDriverWait(driver, 3).until(EC.new_window_is_opened(handles))
    handles = driver.window_handles
    handles.remove(current)
    driver.switch_to.window(handles[0])
    element = driver.find_element(By.TAG_NAME, "body")
    driver.close()

    with pytest.raises(NoSuchWindowException):
        element.text


def testClickingOnAButtonThatClosesAnOpenWindowDoesNotCauseTheBrowserToHang(driver, pages):
    pages.load("xhtmlTest.html")
    current = driver.current_window_handle
    handles = driver.window_handles
    driver.find_element(By.NAME, "windowThree").click()
    WebDriverWait(driver, 3).until(EC.new_window_is_opened(handles))
    handles = driver.window_handles
    handles.remove(current)
    driver.switch_to.window(handles[0])
    driver.find_element(By.ID, "close").click()
    driver.switch_to.window(current)
    driver.find_element(By.ID, "linkId")


@pytest.mark.xfail_safari
def testCanCallGetWindowHandlesAfterClosingAWindow(driver, pages):
    pages.load("xhtmlTest.html")
    current = driver.current_window_handle
    handles = driver.window_handles
    driver.find_element(By.NAME, "windowThree").click()
    WebDriverWait(driver, 3).until(EC.new_window_is_opened(handles))
    handles = driver.window_handles
    handles.remove(current)
    driver.switch_to.window(handles[0])

    driver.find_element(By.ID, "close").click()
    WebDriverWait(driver, 3).until(EC.number_of_windows_to_be(1))


def testCanObtainAWindowHandle(driver, pages):
    pages.load("xhtmlTest.html")
    currentHandle = driver.current_window_handle
    assert currentHandle is not None


def testFailingToSwitchToAWindowLeavesTheCurrentWindowAsIs(driver, pages):
    pages.load("xhtmlTest.html")
    current = driver.current_window_handle
    with pytest.raises(NoSuchWindowException):
        driver.switch_to.window("I will never exist")
    new_handle = driver.current_window_handle
    assert current == new_handle


@pytest.mark.xfail_safari
def testThatAccessingFindingAnElementAfterWindowIsClosedAndHaventswitchedDoesntCrash(driver, pages):
    pages.load("xhtmlTest.html")
    current = driver.current_window_handle
    handles = driver.window_handles
    driver.find_element(By.NAME, "windowThree").click()
    WebDriverWait(driver, 3).until(EC.new_window_is_opened(handles))
    handles = driver.window_handles
    handles.remove(current)
    driver.switch_to.window(handles[0])

    with pytest.raises(WebDriverException):
        driver.find_element(By.ID, "close").click()
        all_handles = driver.window_handles
        assert 1 == len(all_handles)
        driver.find_element(By.ID, "close")
    driver.switch_to.window(current)


@pytest.mark.xfail_ie
def testShouldBeAbleToCreateANewWindow(driver, pages):
    original_handle = driver.current_window_handle

    driver.switch_to.new_window(WindowTypes.TAB)
    new_handle = driver.current_window_handle

    driver.close()
    driver.switch_to.window(original_handle)

    assert new_handle != original_handle
