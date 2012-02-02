#ifndef UPNPDIALOG_H 
#define UPNPDIALOG_H

#include <QDialog>
#include "ui_upnpdialog.h"
#include "upnp.h"

class UPnPDialog: public QDialog, public Ui::UPnPDialog
{
	Q_OBJECT

public:
	UPnPDialog ( QWidget *parent = 0 );

public slots:
	void slot_newRouterDiscovered ( int num );
	void slot_newForwarding ( const QString &ip, const std::vector<UPnPForward> &fwds );
	void slot_addClicked ();
	void slot_removeClicked ();

private:
	UPnP m_upnp;
	QList<QString> m_treewidgetheaders;
};


#endif
