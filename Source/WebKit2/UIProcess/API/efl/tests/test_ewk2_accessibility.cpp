#include "config.h"

#if HAVE(ACCESSIBILITY) && defined(HAVE_ECORE_X)

#include "UnitTestUtils/EWK2UnitTestBase.h"
#include <Ecore_Evas.h>
#include <Ecore_X.h>
#include <WebCore/EflScreenUtilities.h>

using namespace EWK2UnitTest;

class EWK2Accessibility : public EWK2UnitTestBase {
protected:
    Ecore_X_Window xwindow()
    {
        return WebCore::getEcoreXWindow(ecore_evas_ecore_evas_get(evas_object_evas_get(webView())));
    }
};

TEST_F(EWK2Accessibility, DISABLED_ewk_accessibility_action_activate)
{
    ecore_x_e_illume_access_action_activate_send(xwindow());
    bool activateSend = false;
    waitUntilTrue(activateSend, 1);
    ASSERT_TRUE(ewk_view_accessibility_action_activate_get(webView()));
}

TEST_F(EWK2Accessibility, DISABLED_ewk_accessibility_action_next)
{
    ecore_x_e_illume_access_action_read_next_send(xwindow());
    bool nextSend = false;
    waitUntilTrue(nextSend, 1);
    ASSERT_TRUE(ewk_view_accessibility_action_next_get(webView()));
}

TEST_F(EWK2Accessibility, DISABLED_ewk_accessibility_action_prev)
{
    ecore_x_e_illume_access_action_read_prev_send(xwindow());
    bool prevSend = false;
    waitUntilTrue(prevSend, 1);
    ASSERT_TRUE(ewk_view_accessibility_action_prev_get(webView()));
}

TEST_F(EWK2Accessibility, DISABLED_ewk_accessibility_action_read_by_point)
{
    ecore_x_e_illume_access_action_read_send(xwindow());
    bool readSend = false;
    waitUntilTrue(readSend, 1);
    ASSERT_TRUE(ewk_view_accessibility_action_read_by_point_get(webView()));
}

#endif // HAVE(ACCESSIBILITY) && defined(HAVE_ECORE_X)
