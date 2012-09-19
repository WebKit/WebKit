/*
    Copyright (C) 2012 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef EWKTestView_h
#define EWKTestView_h

#include <Ecore_Evas.h>
#include <Evas.h>
#include <string>
#include <wtf/OwnPtr.h>
#include <wtf/efl/RefPtrEfl.h>

namespace EWKUnitTests {

class EWKTestEcoreEvas {
public:
    EWKTestEcoreEvas(int useX11Window);
    EWKTestEcoreEvas(const char* engine_name, int viewport_x, int viewport_y, int viewport_w, int viewport_h, const char* extra_options, int useX11Window);

    Evas* evas();
    void show();

private:
    OwnPtr<Ecore_Evas> m_ecoreEvas;
};

class EWKTestView {
public:
    enum EwkViewType {
        SingleView = 0,
        TiledView,
    };

    explicit EWKTestView(Evas*);
    EWKTestView(Evas*, const char* url);
    EWKTestView(Evas*, EwkViewType, const char* url);
    EWKTestView(Evas*, EwkViewType, const char* url, int width, int height);

    Evas_Object* webView() { return m_webView.get(); }
    Evas_Object* mainFrame();
    Evas* evas();
    void show();

    bool init();
    void bindEvents(void (*callback)(void*, Evas_Object*, void*), const char* eventName, void* ptr);

private:
    EWKTestView(const EWKTestView&);
    EWKTestView operator=(const EWKTestView&);

    Evas* m_evas;
    RefPtr<Evas_Object> m_webView;

    int m_width, m_height;
    EwkViewType m_defaultViewType;
    std::string m_url;
};

}

#endif
