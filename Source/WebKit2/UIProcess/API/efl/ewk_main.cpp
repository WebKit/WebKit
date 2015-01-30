/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2011, 2014 Samsung Electronics
    Copyright (C) 2012 Intel Corporation

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

#include "config.h"
#include "ewk_main.h"

#include "EwkDebug.h"
#include "ewk_main_private.h"
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_IMF.h>
#include <Edje.h>
#include <Efreet.h>
#include <Eina.h>
#include <Evas.h>

#ifdef HAVE_ECORE_X
#include <Ecore_X.h>
#endif

#if ENABLE(BATTERY_STATUS)
#include <Eldbus.h>
#endif

namespace WebKit {

enum class EFLModuleInitFailure {
    EinaLog,
    Evas,
    Ecore,
    EcoreEvas,
    EcoreImf,
    Efreet,
    EcoreX,
    Edje,
#if ENABLE(BATTERY_STATUS)
    Eldbus
#endif
};

EwkMain::EwkMain()
    : m_initCount(0)
    , m_logDomainId(-1)
{
}

EwkMain& EwkMain::singleton()
{
    static EwkMain instance;
    return instance;
}

EwkMain::~EwkMain()
{
    if (m_initCount > 0)
        WARN("EWebkit has not been destroyed. You should call ewk_shutdown().");
}

int EwkMain::initialize()
{
    if (m_initCount)
        return ++m_initCount;

    if (!eina_init()) {
        EINA_LOG_CRIT("could not init eina.");
        return 0;
    }

    m_logDomainId = eina_log_domain_register("ewebkit2", EINA_COLOR_ORANGE);
    if (m_logDomainId < 0) {
        EINA_LOG_CRIT("could not register log domain 'ewebkit2'");
        shutdownInitializedEFLModules(EFLModuleInitFailure::EinaLog);
        return 0;
    }

    if (!evas_init()) {
        CRITICAL("could not init evas.");
        shutdownInitializedEFLModules(EFLModuleInitFailure::Evas);
        return 0;
    }

    if (!ecore_init()) {
        CRITICAL("could not init ecore.");
        shutdownInitializedEFLModules(EFLModuleInitFailure::Ecore);
        return 0;
    }

    if (!ecore_evas_init()) {
        CRITICAL("could not init ecore_evas.");
        shutdownInitializedEFLModules(EFLModuleInitFailure::EcoreEvas);
        return 0;
    }

    if (!ecore_imf_init()) {
        CRITICAL("could not init ecore_imf.");
        shutdownInitializedEFLModules(EFLModuleInitFailure::EcoreImf);
        return 0;
    }

    if (!efreet_init()) {
        CRITICAL("could not init efreet.");
        shutdownInitializedEFLModules(EFLModuleInitFailure::Efreet);
        return 0;
    }

#ifdef HAVE_ECORE_X
    if (!ecore_x_init(0)) {
        CRITICAL("could not init ecore_x.");
        shutdownInitializedEFLModules(EFLModuleInitFailure::EcoreX);
        return 0;
    }
#endif

    if (!edje_init()) {
        CRITICAL("Could not init edje.");
        shutdownInitializedEFLModules(EFLModuleInitFailure::Edje);
        return 0;
    }

#if ENABLE(BATTERY_STATUS)
    if (!eldbus_init()) {
        CRITICAL("Could not init eldbus.");
        shutdownInitializedEFLModules(EFLModuleInitFailure::Eldbus);
        return 0;
    }
#endif
    if (!ecore_main_loop_glib_integrate()) {
        WARN("Ecore was not compiled with GLib support, some plugins will not "
            "work (ie: Adobe Flash)");
    }

    return ++m_initCount;
}

int EwkMain::finalize()
{
    if (--m_initCount)
        return m_initCount;

#if ENABLE(BATTERY_STATUS)
    eldbus_shutdown();
#endif
    edje_shutdown();
#ifdef HAVE_ECORE_X
    ecore_x_shutdown();
#endif
    efreet_shutdown();
    ecore_imf_shutdown();
    ecore_evas_shutdown();
    ecore_shutdown();
    evas_shutdown();
    eina_log_domain_unregister(m_logDomainId);
    m_logDomainId = -1;
    eina_shutdown();

    return 0;
}

void EwkMain::shutdownInitializedEFLModules(EFLModuleInitFailure module)
{
    switch (module) {
#if ENABLE(BATTERY_STATUS)
    case EFLModuleInitFailure::Eldbus:
        eldbus_shutdown();
#endif
    case EFLModuleInitFailure::Edje:
#ifdef HAVE_ECORE_X
        ecore_x_shutdown();
#endif
    case EFLModuleInitFailure::EcoreX:
        efreet_shutdown();
    case EFLModuleInitFailure::Efreet:
        ecore_imf_shutdown();
    case EFLModuleInitFailure::EcoreImf:
        ecore_evas_shutdown();
    case EFLModuleInitFailure::EcoreEvas:
        ecore_shutdown();
    case EFLModuleInitFailure::Ecore:
        evas_shutdown();
    case EFLModuleInitFailure::Evas:
        eina_log_domain_unregister(m_logDomainId);
        m_logDomainId = -1;
    case EFLModuleInitFailure::EinaLog:
        eina_shutdown();
    }
}

} // namespace WebKit

using namespace WebKit;

int ewk_init()
{
    return EwkMain::singleton().initialize();
}

int ewk_shutdown()
{
    return EwkMain::singleton().finalize();
}
