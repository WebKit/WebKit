import pytest

from webdriver.bidi.modules.script import ContextTarget

from tests.support.sync import AsyncPoll

pytestmark = pytest.mark.asyncio


@pytest.fixture
def frame_urls(bidi_session):
    async def frame_urls(context):
        contexts = await bidi_session.browsing_context.get_tree(root=context)
        return [child["url"] for child in contexts[0]["children"] or []]

    return frame_urls


@pytest.fixture
def wait_for_frame_urls(bidi_session, configuration, frame_urls):
    async def wait_for_frame_urls(context, expected_urls, timeout=2):
        async def check_urls(_):
            # The order of a context's children isn't defined yet.
            assert sorted(await frame_urls(context)) == sorted(expected_urls)

        wait = AsyncPoll(bidi_session, timeout=timeout * configuration["timeout_multiplier"])
        await wait.until(check_urls)

    return wait_for_frame_urls


@pytest.fixture
def wait_for_frame_url_in_tree(bidi_session, configuration, frame_urls):
    async def wait_for_frame_url_in_tree(context, expected_url, timeout=2):
        async def check_url(_):
            assert expected_url in await frame_urls(context)

        wait = AsyncPoll(bidi_session, timeout=timeout * configuration["timeout_multiplier"])
        await wait.until(check_url)

    return wait_for_frame_url_in_tree


@pytest.fixture
def navigate_frame(bidi_session, wait_for_frame_urls):
    async def navigate_frame(context, selector, url, expected_urls):
        # Navigating a frame from its parent adds a session history entry.
        await bidi_session.script.evaluate(
            expression=f"document.querySelector('{selector}').src = '{url}'",
            target=ContextTarget(context),
            await_promise=False,
        )
        await wait_for_frame_urls(context, expected_urls)

    return navigate_frame


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_navigated_frame(
    bidi_session, current_url, new_tab, inline, wait_for_frame_urls, navigate_frame, domain
):
    frame_urls = [
        inline("<div>frame 1</div>", domain=domain),
        inline("<div>frame 2</div>", domain=domain),
    ]
    page_url = inline(f"<iframe src='{frame_urls[0]}'></iframe>")
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="complete"
    )
    await navigate_frame(new_tab["context"], "iframe", frame_urls[1], [frame_urls[1]])

    await bidi_session.browsing_context.traverse_history(
        context=new_tab["context"], delta=-1
    )
    await wait_for_frame_urls(new_tab["context"], [frame_urls[0]])
    assert await current_url(new_tab["context"]) == page_url

    await bidi_session.browsing_context.traverse_history(
        context=new_tab["context"], delta=1
    )
    await wait_for_frame_urls(new_tab["context"], [frame_urls[1]])
    assert await current_url(new_tab["context"]) == page_url


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_navigated_frame_with_child_frame(
    bidi_session, new_tab, inline, wait_for_frame_urls, navigate_frame, domain
):
    child_url = inline("<div>child</div>", domain=domain)
    frame_urls = [
        inline(f"<div>frame 1</div><iframe src='{child_url}'></iframe>", domain=domain),
        inline(f"<div>frame 2</div><iframe src='{child_url}'></iframe>", domain=domain),
    ]
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=inline(f"<iframe src='{frame_urls[0]}'></iframe>"), wait="complete"
    )
    await navigate_frame(new_tab["context"], "iframe", frame_urls[1], [frame_urls[1]])

    await bidi_session.browsing_context.traverse_history(
        context=new_tab["context"], delta=-1
    )
    await wait_for_frame_urls(new_tab["context"], [frame_urls[0]])


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_two_navigated_frames(
    bidi_session, new_tab, inline, wait_for_frame_urls, navigate_frame, domain
):
    first_frame_urls = [inline("<div>a1</div>", domain=domain), inline("<div>a2</div>", domain=domain)]
    second_frame_urls = [inline("<div>b1</div>", domain=domain), inline("<div>b2</div>", domain=domain)]
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"],
        url=inline(f"<iframe id=a src='{first_frame_urls[0]}'></iframe><iframe id=b src='{second_frame_urls[0]}'></iframe>"),
        wait="complete",
    )
    await navigate_frame(new_tab["context"], "#a", first_frame_urls[1], [first_frame_urls[1], second_frame_urls[0]])
    await navigate_frame(new_tab["context"], "#b", second_frame_urls[1], [first_frame_urls[1], second_frame_urls[1]])

    await bidi_session.browsing_context.traverse_history(
        context=new_tab["context"], delta=-2
    )
    await wait_for_frame_urls(new_tab["context"], [first_frame_urls[0], second_frame_urls[0]])

    await bidi_session.browsing_context.traverse_history(
        context=new_tab["context"], delta=2
    )
    await wait_for_frame_urls(new_tab["context"], [first_frame_urls[1], second_frame_urls[1]])


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_fragment_navigation_in_frame(
    bidi_session, new_tab, inline, wait_for_frame_urls, navigate_frame, domain
):
    frame_url = inline("<div>frame</div>", domain=domain)
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=inline(f"<iframe src='{frame_url}'></iframe>"), wait="complete"
    )
    await navigate_frame(new_tab["context"], "iframe", frame_url + "#foo", [frame_url + "#foo"])

    await bidi_session.browsing_context.traverse_history(
        context=new_tab["context"], delta=-1
    )
    await wait_for_frame_urls(new_tab["context"], [frame_url])


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_navigated_frame_and_main_frame_fragment(
    bidi_session, current_url, new_tab, inline, wait_for_frame_urls, navigate_frame, domain
):
    frame_urls = [
        inline("<div>frame 1</div>", domain=domain),
        inline("<div>frame 2</div>", domain=domain),
    ]
    page_url = inline(f"<iframe src='{frame_urls[0]}'></iframe>")
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="complete"
    )
    await navigate_frame(new_tab["context"], "iframe", frame_urls[1], [frame_urls[1]])
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url + "#foo", wait="complete"
    )

    # One traversal moves the main frame within its document and the frame to another document.
    await bidi_session.browsing_context.traverse_history(
        context=new_tab["context"], delta=-2
    )
    await wait_for_frame_urls(new_tab["context"], [frame_urls[0]])
    assert await current_url(new_tab["context"]) == page_url


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_navigated_frame_with_sibling_without_src(
    bidi_session, new_tab, inline, wait_for_frame_url_in_tree, navigate_frame, domain
):
    # The sibling's first document is the initial about:blank, which its navigation replaces.
    frame_urls = [
        inline("<div>frame 1</div>", domain=domain),
        inline("<div>frame 2</div>", domain=domain),
    ]
    sibling_url = inline("<div>sibling</div>", domain=domain)
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"],
        url=inline(f"<iframe id=a src='{frame_urls[0]}'></iframe><iframe id=b></iframe>"),
        wait="complete",
    )
    await navigate_frame(new_tab["context"], "#a", frame_urls[1], [frame_urls[1], "about:blank"])
    await navigate_frame(new_tab["context"], "#b", sibling_url, [frame_urls[1], sibling_url])

    await bidi_session.browsing_context.traverse_history(
        context=new_tab["context"], delta=-1
    )
    await wait_for_frame_url_in_tree(new_tab["context"], frame_urls[0])


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_navigated_frame_after_inserting_frame_before_it(
    bidi_session, new_tab, inline, wait_for_frame_urls, navigate_frame, domain
):
    frame_urls = [
        inline("<div>frame 1</div>", domain=domain),
        inline("<div>frame 2</div>", domain=domain),
    ]
    inserted_url = inline("<div>inserted</div>", domain=domain)
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=inline(f"<iframe src='{frame_urls[0]}'></iframe>"), wait="complete"
    )
    await navigate_frame(new_tab["context"], "iframe", frame_urls[1], [frame_urls[1]])
    await bidi_session.script.evaluate(
        expression=f"""{{
            const frame = document.createElement('iframe');
            frame.src = '{inserted_url}';
            document.body.prepend(frame);
        }}""",
        target=ContextTarget(new_tab["context"]),
        await_promise=False,
    )
    await wait_for_frame_urls(new_tab["context"], [inserted_url, frame_urls[1]])

    await bidi_session.browsing_context.traverse_history(
        context=new_tab["context"], delta=-1
    )
    await wait_for_frame_urls(new_tab["context"], [inserted_url, frame_urls[0]])
