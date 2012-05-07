/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 University of Szeged. All rights reserved.
 *
 * GNU Lesser General Public License Usage
 * This file may be used under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 * Please review the following information to ensure the GNU Lesser
 * General Public License version 2.1 requirements will be met:
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU General
 * Public License version 3.0 as published by the Free Software Foundation.
 * Please review the following information to ensure the GNU General Public
 * License version 3.0 requirements will be met:
 * http://www.gnu.org/copyleft/gpl.html.
 *
 */

#ifndef TestFontDatabase_h
#define TestFontDatabase_h

#include <QFont>
#include <QtPlatformSupport/private/qcoretextfontdatabase_p.h>

class TestFontDatabase : public QCoreTextFontDatabase {
private:
    virtual void populateFontDatabase();
    virtual QFont defaultFont() const;
};

#endif
