#include <QtGui>

#include "upnpdialog.h"

using namespace std;

int main( int argc, char *argv[] )
{
	QApplication app( argc, argv );
	
	QTranslator translator;
	translator.load("upnpcontrol_de");
	app.installTranslator(&translator);

	UPnPDialog dialog;
	dialog.show();
	
	return app.exec();
}
