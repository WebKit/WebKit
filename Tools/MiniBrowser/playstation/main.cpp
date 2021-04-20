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

    loadLibraryOrExit("libpng16");
    loadLibraryOrExit("libicu");
    loadLibraryOrExit("libfontconfig");
    loadLibraryOrExit("libfreetype");
    loadLibraryOrExit("libharfbuzz");
    loadLibraryOrExit("libcairo");
    loadLibraryOrExit("libToolKitten");    
    loadLibraryOrExit("libSceNKWebKitRequirements");
    loadLibraryOrExit("libJavaScriptCore");
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

int main(int, char *[])
{
    WKRunLoopInitializeMain();

    ApplicationClient applicationClient;
    auto& app = Application::singleton();
    app.init(&applicationClient);

    auto mainWindow = std::make_unique<MainWindow>();
    mainWindow->setFocused();
    app.setRootWidget(move(mainWindow));

    // Request the first frame to start the application loop.
    applicationClient.requestNextFrame();

    WKRunLoopRunMain();

    return 0;
}
