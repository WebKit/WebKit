/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
// Functions and macros that really need to be in QTestLib

#include <QEventLoop>
#include <QSignalSpy>
#include <QTimer>

#if !defined(TESTS_SOURCE_DIR)
#define TESTS_SOURCE_DIR ""
#endif

void addQtWebProcessToPath();
bool waitForSignal(QObject*, const char* signal, int timeout = 10000);
void suppressDebugOutput();

#define QTWEBKIT_API_TEST_MAIN(TestObject) \
int main(int argc, char** argv) \
{ \
    suppressDebugOutput(); \
    QApplication app(argc, argv); \
    QTEST_DISABLE_KEYPAD_NAVIGATION \
    TestObject tc; \
    return QTest::qExec(&tc, argc, argv); \
}
