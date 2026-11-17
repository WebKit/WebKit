// Every edge has a different inset, so that the results show which edges are within the safe area.
var safeAreaInsets = { top: 10, right: 20, bottom: 30, left: 40 };
var viewportWidthWithoutSafeArea;
var viewportHeightWithoutSafeArea;
var adjustedContentInset;
var safeAreaInsetStyle;

async function setSafeAreaInsets(insets = safeAreaInsets)
{
    if (viewportWidthWithoutSafeArea === undefined) {
        // Make the page taller than the viewport. Otherwise, the bottom of the content can cut the unobscured rect, and
        // with it innerHeight, short of the bottom of the viewport, depending on when the content was last resized.
        const tallElement = document.createElement("div");
        tallElement.style.height = "2000px";
        document.body.appendChild(tallElement);
        await UIHelper.ensurePresentationUpdate();

        viewportWidthWithoutSafeArea = innerWidth;
        viewportHeightWithoutSafeArea = innerHeight;
    }

    safeAreaInsets = insets;
    debug(`Set the safe area insets to top ${insets.top}, right ${insets.right}, bottom ${insets.bottom} and left ${insets.left}`);
    await UIHelper.setSafeAreaInsets(insets.top, insets.right, insets.bottom, insets.left);
}

async function setViewport(content)
{
    debug(`Set the viewport to "${content}"`);
    document.querySelector("meta[name=viewport]").setAttribute("content", content);
    await UIHelper.ensurePresentationUpdate();
    await UIHelper.ensureVisibleContentRectAndStablePresentationUpdate();
}

function safeAreaInsetElement()
{
    let element = document.getElementById("safe-area-inset");
    if (!element) {
        element = document.createElement("div");
        element.id = "safe-area-inset";
        element.style.padding = "env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)";
        document.body.appendChild(element);
    }
    return element;
}

// Checks that the page lays out within the safe area at the given edges, and extends underneath it at the others:
// the scroll view only insets its content at the given edges, and env(safe-area-inset-*) is only non-zero at the others.
async function shouldBeWithinSafeAreaAt(edges)
{
    debug(`Within the safe area at: ${edges.length ? edges.join(", ") : "no edges"}`);

    adjustedContentInset = await UIHelper.adjustedContentInset();
    safeAreaInsetStyle = getComputedStyle(safeAreaInsetElement());
    for (const edge of ["top", "right", "bottom", "left"]) {
        const isWithinSafeArea = edges.includes(edge);
        shouldBe(`adjustedContentInset.${edge}`, isWithinSafeArea ? `${safeAreaInsets[edge]}` : "0");
        const property = `padding${edge[0].toUpperCase()}${edge.slice(1)}`;
        shouldBeEqualToString(`safeAreaInsetStyle.${property}`, isWithinSafeArea ? "0px" : `${safeAreaInsets[edge]}px`);
    }

    // Only the top, left and right edges of the safe area obscure the page by default.
    const obscuredWidth = (edges.includes("left") ? safeAreaInsets.left : 0) + (edges.includes("right") ? safeAreaInsets.right : 0);
    const obscuredHeight = edges.includes("top") ? safeAreaInsets.top : 0;
    shouldBe("innerWidth", obscuredWidth ? `viewportWidthWithoutSafeArea - ${obscuredWidth}` : "viewportWidthWithoutSafeArea");
    shouldBe("innerHeight", obscuredHeight ? `viewportHeightWithoutSafeArea - ${obscuredHeight}` : "viewportHeightWithoutSafeArea");
    debug("");
}
