import time

import pytest
from webdriver import error

from tests.support.classic.asserts import assert_success
from tests.support.sync import Poll


def forward(session):
    return session.transport.send(
        "POST", "session/{session_id}/forward".format(**vars(session)))


def frame_url(session, frame):
    session.switch_to_frame(frame)
    try:
        return session.execute_script("return document.URL;")
    finally:
        session.switch_to_parent_frame()


def wait_for_frame_url(session, frame, url, message):
    # The frame may be navigating while it's polled.
    Poll(session, ignored_exceptions=error.WebDriverException, message=message).until(
        lambda s: frame_url(s, frame) == url)


def navigate_frame(session, frame, url):
    # Navigating the frame from its parent adds a session history entry.
    session.execute_script("arguments[0].src = arguments[1];", args=[frame, url])
    wait_for_frame_url(session, frame, url, "The frame did not navigate")


def slow_page(inline, url, html, domain):
    # The page loads a slow image with a URL of its own each time, so that no cache makes a load fast.
    image_url = url("/webdriver/tests/support/html/default.html", domain=domain, query="pipe=trickle(d1)")
    return inline("""{}<script>
        const image = document.createElement('img');
        image.src = '{}&' + Math.random();
        document.body.append(image);
    </script>""".format(html, image_url), domain=domain)


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
def test_navigated_iframe(session, inline, domain):
    frame_pages = [
        inline("<p>frame 1</p>", domain=domain),
        inline("<p>frame 2</p>", domain=domain),
    ]
    page = inline("<iframe src='{}'></iframe>".format(frame_pages[0]))

    session.url = page
    frame = session.find.css("iframe", all=False)
    navigate_frame(session, frame, frame_pages[1])
    session.back()
    wait_for_frame_url(session, frame, frame_pages[0], "Back did not go back")

    response = forward(session)
    assert_success(response)

    assert session.url == page
    wait_for_frame_url(session, frame, frame_pages[1], "The frame did not go forward")


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
def test_navigated_iframe_with_child_frame(session, inline, domain):
    child_page = inline("<p>child</p>", domain=domain)
    frame_pages = [
        inline("<p>frame 1</p><iframe src='{}'></iframe>".format(child_page), domain=domain),
        inline("<p>frame 2</p><iframe src='{}'></iframe>".format(child_page), domain=domain),
    ]
    page = inline("<iframe src='{}'></iframe>".format(frame_pages[0]))

    session.url = page
    frame = session.find.css("iframe", all=False)
    navigate_frame(session, frame, frame_pages[1])
    session.back()
    wait_for_frame_url(session, frame, frame_pages[0], "Back did not go back")

    response = forward(session)
    assert_success(response)

    assert session.url == page
    wait_for_frame_url(session, frame, frame_pages[1], "The frame did not go forward")


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
def test_fragment_navigation_in_iframe(session, inline, domain):
    frame_page = inline("<p>frame</p>", domain=domain)
    frame_pages = [frame_page, frame_page + "#foo"]
    page = inline("<iframe src='{}'></iframe>".format(frame_pages[0]))

    session.url = page
    frame = session.find.css("iframe", all=False)
    navigate_frame(session, frame, frame_pages[1])
    session.back()
    wait_for_frame_url(session, frame, frame_pages[0], "Back did not go back")

    response = forward(session)
    assert_success(response)

    assert session.url == page
    wait_for_frame_url(session, frame, frame_pages[1], "The frame did not go forward")


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
def test_waits_for_navigated_iframe_to_load(session, inline, url, domain):
    # "If the previous step completed results in a pageHide event firing, wait until pageShow event fires": the
    # document that Forward loads in the frame fires pageshow after its load event.
    frame_pages = [
        inline("<p>frame 1</p>", domain=domain),
        slow_page(inline, url, "<p>frame 2</p>", domain),
    ]
    page = inline("<iframe src='{}'></iframe>".format(frame_pages[0]))

    session.url = page
    frame = session.find.css("iframe", all=False)
    navigate_frame(session, frame, frame_pages[1])
    session.back()
    wait_for_frame_url(session, frame, frame_pages[0], "Back did not go back")

    start = time.monotonic()
    response = forward(session)
    assert_success(response)
    # The image takes a second to load, so a Forward that waits for the frame's load takes at least that long. Checking
    # the frame's state after the command doesn't tell, since a command in the frame waits for its load to end.
    assert time.monotonic() - start >= 1

    session.switch_to_frame(frame)
    assert session.execute_script("return [document.URL, document.readyState];") == [frame_pages[1], "complete"]
