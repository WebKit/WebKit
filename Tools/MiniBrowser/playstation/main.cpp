/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif
#include "MainWindow.h"
#include <WebKit/WKRunLoop.h>
#include <dlfcn.h>
#include <toolkitten/Application.h>

using toolkitten::Widget;
using toolkitten::Application;

static void loadLibraryOrExit(const char* name)
{
    if (!dlopen(name, RTLD_NOW)) {
        fprintf(stderr, "Failed to load %s.\n", name);
        exit(EXIT_FAILURE);
    }
}

__attribute__((constructor(110)))
static void initialize()
{
    loadLibraryOrExit("PosixWebKit");
    setenv_np("WebInspectorServerPort", "868", 1);

    loadLibraryOrExit(ICU_LOAD_AT);
    loadLibraryOrExit(PNG_LOAD_AT);
#if defined(JPEG_LOAD_AT)
    loadLibraryOrExit(JPEG_LOAD_AT);
#endif 
#if defined(WebP_LOAD_AT)
    loadLibraryOrExit(WebP_LOAD_AT);
#endif
    loadLibraryOrExit(Fontconfig_LOAD_AT);
    loadLibraryOrExit(Freetype_LOAD_AT);
    loadLibraryOrExit(HarfBuzz_LOAD_AT);
    loadLibraryOrExit(Cairo_LOAD_AT);
    loadLibraryOrExit(ToolKitten_LOAD_AT);
    loadLibraryOrExit(WebKitRequirements_LOAD_AT);
#if defined(WPE_LOAD_AT)
    loadLibraryOrExit(WPE_LOAD_AT);
#endif
#if !(defined(ENABLE_STATIC_JSC) && ENABLE_STATIC_JSC)
    loadLibraryOrExit("libJavaScriptCore");
#endif
    loadLibraryOrExit("libWebKit");
}

class ApplicationClient : public Application::Client {
public:
    void requestNextFrame() override
    {
        WKRunLoopCallOnMainThread(updateApplication, this);
    }

private:
    static void updateApplication(void*)
    {
        Application::singleton().update();
    }
};

int main(int argc, char *argv[])
{
    WKRunLoopInitializeMain();

    ApplicationClient applicationClient;
    auto& app = Application::singleton();
    app.init(&applicationClient);

    const std::vector<std::string> options(argv + 1, argv + argc);
    auto mainWindow = std::make_unique<MainWindow>(options);
    mainWindow->setFocused();
    app.setRootWidget(std::move(mainWindow));

    // Request the first frame to start the application loop.
    applicationClient.requestNextFrame();

    WKRunLoopRunMain();

    return 0;
}
