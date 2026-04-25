function window_state_context(t) {
    let rect = null;
    let state = "restored";
    t.add_cleanup(async () => {
        if (state === "minimized") await restore();
    });
    async function restore() {
        if (state !== "minimized") return;
        state = "restoring";
        await test_driver.set_window_rect(rect);
        state = "restored";
    }

    async function minimize() {
        state = "minimized";
        rect = await test_driver.minimize_window();
    }

    return { minimize, restore };
}
