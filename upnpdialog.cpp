#include "upnpdialog.h"
#include "upnp.h"
#include "upnprouter.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>

UPnPDialog::UPnPDialog ( QWidget* parent )
	: QDialog( parent )
{
	setupUi( this );

	connect( &m_upnp, SIGNAL(newRouterDiscovered(int)), this, SLOT(slot_newRouterDiscovered(int)) );
	connect( &m_upnp, SIGNAL(newRouterForwarding(const QString &, const std::vector<UPnPForward> &)), this, SLOT(slot_newForwarding(const QString &, const std::vector<UPnPForward> &)) );
	connect( button_newEntry, SIGNAL(clicked()), this, SLOT(slot_addClicked()) );
	connect( button_remove, SIGNAL(clicked()), this, SLOT(slot_removeClicked()) );

	m_treewidgetheaders << tr("Remote Host")     << tr("External Port") << tr("Protocol")    << tr("Internal Port")
	                    << tr("Internal Client") << tr("Enabled")       << tr("Description") << tr("Lease Duration");
	
	m_upnp.discover();	
}

void UPnPDialog::slot_newRouterDiscovered ( int num )
{
	if( num == 0 )
		routertabs->clear();

	QTreeWidget *tw = new QTreeWidget;

	tw->setColumnCount( 8 );
	tw->setHeaderLabels( m_treewidgetheaders );
	routertabs->addTab( tw, m_upnp[num]->getIP() );
}


void UPnPDialog::slot_addClicked ()
{
	button_newEntry->setDisabled(true);

	QString ip = routertabs->tabText( routertabs->currentIndex() );
	qDebug() << tr("+++++ clicked add for router: %1").arg(ip);
	
	UPnPForward fwd;
	fwd.m_remoteHost     = (edit_remoteHost->text() == "any") ? "" : edit_remoteHost->text();
	fwd.m_externalPort   = edit_externalPort->text();
	fwd.m_protocol       = edit_Protocol->currentText();
	fwd.m_internalPort   = edit_internalPort->text();
	fwd.m_internalClient = edit_internalClient->text();
	fwd.m_enabled        = checkBox_enabled->isChecked() ? "1" : "0";
	fwd.m_description    = edit_description->text();
	fwd.m_leaseDuration  = (edit_leaseDuration->text() == "") ? "0" : edit_leaseDuration->text() ;

	m_upnp.addForward( ip, fwd );

	button_newEntry->setEnabled(true);
}

void UPnPDialog::slot_removeClicked()
{
	button_remove->setDisabled(true);
	
	QList<QTreeWidgetItem *> list = ((QTreeWidget*)routertabs->widget(routertabs->currentIndex()))->selectedItems();
	if(list.isEmpty())
	{
		button_remove->setEnabled(true);
		return;
	}
	QString ip = routertabs->tabText( routertabs->currentIndex() );
	UPnPForward fwd;
	
	fwd.m_remoteHost     = (list[0]->text(0) == "any") ? "" : list[0]->text(0);
	fwd.m_externalPort   = list[0]->text(1);
	fwd.m_protocol       = list[0]->text(2);
	fwd.m_internalPort   = list[0]->text(3);
       	fwd.m_internalClient = list[0]->text(4);
	fwd.m_enabled        = list[0]->text(5);
       	fwd.m_description    = list[0]->text(6);
      	fwd.m_leaseDuration  = list[0]->text(7);

	m_upnp.removeForward( ip, fwd );
	
	button_remove->setEnabled(true);
}


void UPnPDialog::slot_newForwarding ( const QString &ip, const std::vector<UPnPForward> &fwds )
{
	// insert fwd into QTreeWidget 
	int tabnum;
	for( tabnum = 0; tabnum < routertabs->count(); ++tabnum )
	{
		if( routertabs->tabText(tabnum) == ip )
			break;
	}

	if( routertabs->tabText(tabnum) != ip )
	{
		qDebug() << tr("+++++ we got a forwarding for a router that we didn't know of Oo");
		return;
	}

	((QTreeWidget*)routertabs->widget(tabnum))->clear();
	foreach( UPnPForward fwd, fwds )
	{
		QTreeWidgetItem *entry = new QTreeWidgetItem( ((QTreeWidget*)routertabs->widget(tabnum)) );
		entry->setText( 0, (fwd.m_remoteHost == "") ? "any" : fwd.m_remoteHost );
		entry->setText( 1, fwd.m_externalPort );
		entry->setText( 2, fwd.m_protocol );
		entry->setText( 3, fwd.m_internalPort );
		entry->setText( 4, fwd.m_internalClient );
		entry->setText( 5, fwd.m_enabled );
		entry->setText( 6, fwd.m_description );
		entry->setText( 7, fwd.m_leaseDuration );
	}
}

