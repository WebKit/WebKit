/*
 * Copyright (C) 2014 Samsung Electronics
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

/**
 * @mainpage EWebKit2
 *
 * @section intro What is EWebKit2?
 *
 * This is the web engine for the Enlightenment Foundation Libraries, which is based on WebKit(especially WebKit2).
 *
 * EWebKit2 is based on Eina, Ecore and Evas.
 * (EWebKit2 does not require Elementary. If you want to use ewebkit2 in Elementary, consider to use elm_web)
 *
 * And, ewebkit2 may require glib (for internal purpose) because it depends on several glib based components such as libsoup, gstreamer...
 * (at least now).
 *
 * @section start Getting Started.
 *
 * Like other EFL libraries, ewebkit2 also has ewk_init() and ewk_shutdown().
 *
 * You should call ewk_init() before using ewebkit APIs and ewk_shudown() before finishing the process.
 *
 * @code
 * #include <EWebKit2.h>
 *
 * int main(int argc, char **argv)
 * {
 *    ewk_init();
 *    // create window(s) and ewk_view here and do what you want including event loop.
 *    ewk_shutdown();
 *    return 0;
 * }
 * @endcode
 *
 * To compile your application with ewebkit2, you should compile with like below.
 *
 * @code
 * gcc sample.c `pkg-config --cflags --libs ewebkit2`
 * @endcode
 *
 * If you are using CMake, you can include below in your CMakeLists.txt.
 *
 * @code
 * find_package(EWebKit2 REQUIRED)
 * @endcode
 *
 * Below example is simple application which loads enlightenment.org.
 *
 * @code
 * // gcc simple.c `pkg-config --cflags --libs ecore-evas ewebkit2`
 * #include <Evas.h>
 * #include <Ecore_Evas.h>
 * #include <EWebKit2.h>
 *
 * int main()
 * {
 *    Evas *evas;
 *    Ecore_Evas *win;
 *    Evas_Object *ewk;
 *
 *    ewk_init();
 *
 *    win = ecore_evas_new(NULL, 0, 0, 800, 600, NULL);
 *    evas = ecore_evas_get(win);
 *    ecore_evas_show(win);
 *
 *    ewk = ewk_view_add(evas);
 *    ewk_view_url_set(ewk, "http://enlightenment.org");
 *    evas_object_move(ewk, 0, 0);
 *    evas_object_resize(ewk, 800, 600);
 *    evas_object_show(ewk);
 *
 *    ecore_main_loop_begin();
 *
 *    ewk_shutdown();
 *
 *    return 0;
 * }
 * @endcode
 */
