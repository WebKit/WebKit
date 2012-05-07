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

#include "TestIntegration.h"
#include <QPlatformIntegrationPlugin>
#include <QtGlobal>

class Q_DECL_EXPORT TestIntegrationPlugin : public QPlatformIntegrationPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPlatformIntegrationFactoryInterface" FILE "testplatform.json")
public:
    QStringList keys() const;
    QPlatformIntegration* create(const QString&, const QStringList&);

    static void initialize();
};

QStringList TestIntegrationPlugin::keys() const
{
    QStringList list;
    list << QStringLiteral("testplatform");
    return list;
}

QPlatformIntegration* TestIntegrationPlugin::create(const QString& system, const QStringList& parameters)
{
    if (!system.compare(QStringLiteral("testplatform"), Qt::CaseInsensitive))
        return new TestIntegration();

    return 0;
}

#include "main.moc"

void TestIntegrationPlugin::initialize()
{
    QStaticPlugin plugin;
    plugin.instance = &qt_plugin_instance;
    plugin.metaData = &qt_plugin_query_metadata;
    qRegisterStaticPluginFunction(plugin);
}

static void constructorFunction()
{
    TestIntegrationPlugin::initialize();
}

Q_CONSTRUCTOR_FUNCTION(constructorFunction);
