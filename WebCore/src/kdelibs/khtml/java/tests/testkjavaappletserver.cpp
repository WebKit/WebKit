#include <kapp.h>
#include <kcmdlineargs.h>
#include <kjavaappletserver.h>
#include <kjavaapplet.h>
#include <kjavaappletwidget.h>
#include <kdebug.h>
#include <qstring.h>
#include <stdio.h>
#include <unistd.h>


static KCmdLineOptions options[] =
{
    { "+[kdelibs_path]", "path to kdelibs directory", 0 },
    { 0, 0, 0 }
};

int main(int argc, char **argv)
{
    KCmdLineArgs::init( argc, argv, "testKJASSever", "test program", "0.0" );

    KApplication app;

    QString path_to_kdelibs = "/build/wynnw/kde-src";

    KJavaAppletContext* context = new KJavaAppletContext();
    KJavaAppletWidget *a = new KJavaAppletWidget( context );

    a->show();

    a->applet()->setBaseURL( "file:" + path_to_kdelibs + "/kdelibs/khtml/test/" );
    a->applet()->setAppletName( "Lake" );
    a->applet()->setAppletClass( "lake.class" );
    a->applet()->setParameter( "image", "konqi.gif" );

    a->showApplet();
    a->applet()->start();

    app.exec();
}
