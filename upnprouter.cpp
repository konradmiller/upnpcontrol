#include <iostream>
#include <QtNetwork>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QDebug>
#include <QtGui>
#include <QTcpSocket>
#include <QMessageBox>
#include "upnprouter.h"
#include "minigetter.h"

UPnPRouter::UPnPRouter ( const QString &server, const QString &xml_location )
	: QTcpSocket()
	, m_server( server )
	, m_location( xml_location )
	, m_currentForward( 0 )
{
	downloadXmlFile();	
}


UPnPRouter::UPnPRouter ( const UPnPRouter &r )
	: QTcpSocket()
	, m_server( r.m_server )
	, m_location( r.m_location )
	, m_xmldescription( NULL )
	, m_networkaccessmanager( NULL )
	, m_currentForward( 0 )
{ }


UPnPRouter& UPnPRouter::operator= ( const UPnPRouter &r )
{
	m_server 	       = r.m_server;
	m_location 	       = r.m_location;
	m_xmldescription       = NULL;
	m_networkaccessmanager = NULL;

	return *this;
}


QUrl UPnPRouter::getDescriptionUrl ()
{
	return m_location;
}


QString UPnPRouter::getIP ()
{
	return m_location.host();
}


QString UPnPRouter::getControlUrl ()
{
	return m_controlurl;
}


unsigned UPnPRouter::size ()
{
	return m_fwds.size();
}


UPnPForward UPnPRouter::getForward ( unsigned n )
{
	if( n < m_fwds.size() )
		return m_fwds[n];
	else
		return UPnPForward();
}


void UPnPRouter::downloadXmlFile ()
{
	QPointer<MiniGetter> getter = new MiniGetter();
	qDebug() << tr("+++++ getting XML File for %1").arg(m_location.toString());

	connect( getter, SIGNAL(finished(QPointer<MiniGetter>)), this, SLOT(slot_downloadedXmlFile(QPointer<MiniGetter>)) );
	getter->wget( m_location, &m_xmldescription );
}


void UPnPRouter::slot_downloadedXmlFile ( QPointer<MiniGetter> getter )
{
	if( ! getter )
		return;

	if( getter->status() == 200 )
		parseXmlFile();
	else
		qDebug() << tr("+++++ Failed to download XML File: %1").arg(getter->status());
	
	getter->deleteLater();
}

// searches for WANIPConnection or WANPPPConnection and uses one of them, depending on which is available. the forwarding-action is the same for both
void UPnPRouter::parseXmlFile ()
{
//	qDebug() << tr("+++++ PARSING: %1 \n").arg(m_xmldescription);
	int startpos, endpos, wan_pos = 0;

	QRegExp rx( "(<serviceType>[^\\s]*WANIPConnection[^\\s]*</serviceType>)" );
	if( (wan_pos = rx.indexIn(m_xmldescription, wan_pos)) == -1 )
	{
		wan_pos=0;	// reset to start
		rx = QRegExp( "(<serviceType>[^\\s]*WANPPPConnection[^\\s]*</serviceType>)" );
		if( (wan_pos = rx.indexIn(m_xmldescription, wan_pos)) == -1 )
		{
			qDebug() << tr("+++++ xml file does not contain a upnp router description!");
			return;
		}
	}

	rx = QRegExp("(<controlURL>)");
	if( (startpos = rx.indexIn(m_xmldescription, wan_pos)) == -1 )
	{
		qDebug() << tr("+++++ found serviceType, but no controlURL Oo");
		return;
	}
	startpos += 12; // skip the tag

	rx = QRegExp("(</controlURL>)");
	if( (endpos = rx.indexIn(m_xmldescription, startpos)) == -1 )
	{
		qDebug() << tr("+++++ no closing tag found for controlURL");
		return;
	}

	m_controlurl = QString(m_xmldescription).midRef( startpos, endpos-startpos ).toString();
	qDebug() << tr("+++++ got controlurl: %1").arg(m_controlurl);

	rx = QRegExp("WAN");	//parse m_controlurl for servicetype
	startpos = rx.indexIn(m_controlurl,0);
	rx = QRegExp("Connection");
	endpos = rx.indexIn(m_controlurl,0) + 10;

	m_servicetype = QString(m_controlurl).midRef(startpos, endpos-startpos).toString();
//	qDebug() << tr("+++++ ServiceType: %1").arg(m_servicetype);

	downloadFwds();
}

void UPnPRouter::downloadFwds ()
{
	QString query, action;
	QPointer<MiniGetter> getter = new MiniGetter();

	if( m_currentForward == 0 )
		m_fwds.clear();

	action = QString(
		"\"urn:schemas-upnp-org:service:%1:1#GetGenericPortMappingEntry\"" )
		.arg( m_servicetype );

	query = QString(	
		"<?xml version=\"1.0\"?>\r\n"
		"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
		"<s:Body><u:GetGenericPortMappingEntry xmlns:u=\"urn:schemas-upnp-org:service:%1:1\">"
		"<NewPortMappingIndex>%2</NewPortMappingIndex>"
		"</u:GetGenericPortMappingEntry></s:Body></s:Envelope>")
		.arg( m_servicetype )
		.arg( m_currentForward++ );
	
	qDebug() << tr("+++++ Getting Forward # %1 from %2 : %3").arg(m_currentForward-1).arg(m_location.host()).arg(m_location.port());

	connect( getter, SIGNAL(finished(QPointer<MiniGetter>)), this, SLOT(slot_downloadedFwd(QPointer<MiniGetter>)) );
	getter->soap( m_location, QUrl(m_controlurl), action, query, &m_forward_result );

}

void UPnPRouter::slot_downloadedFwd ( QPointer<MiniGetter> getter )
{
	if( ! getter )
		return;

	if( getter->status() == 200 )
	{
		parseFwd( m_forward_result );
		downloadFwds(); // get the next one
	}
	else
	{
		emit newRouterForwarding( this->getIP(), m_fwds );
		qDebug() << tr("+++++ Last PortMapping received from %1").arg( this->getIP() );
	}

	getter->deleteLater();
}


void UPnPRouter::parseFwd ( const QByteArray &soapfwd )
{
//	qDebug() << tr("+++++ parsing soapfwd: %1 \n ").arg(soapfwd);
	QTextStream in( soapfwd );
	QString     str;
	UPnPForward fwd;
	QRegExp     rx;
	QStringList strlist;
	int startpos, endpos;
	bool containsinfos = false;

	strlist.push_back( "NewRemoteHost"             );
	strlist.push_back( "NewExternalPort"           );
	strlist.push_back( "NewProtocol"               );
	strlist.push_back( "NewInternalPort"           );
	strlist.push_back( "NewInternalClient"         );
	strlist.push_back( "NewEnabled"                );
	strlist.push_back( "NewPortMappingDescription" );
	strlist.push_back( "NewLeaseDuration"          );

	while( ! in.atEnd() )
	{
		str = in.readLine();

		for( QStringList::iterator itr = strlist.begin(); itr != strlist.end(); ++itr )
		{
			rx = QRegExp( ("<" + *itr + ".*>") );
			rx.setMinimal( true ); // make match non-greedy

			if( (startpos = rx.indexIn(str, 0)) == -1 )
				continue; 		// tag not found

			startpos += rx.matchedLength(); //itr->length() + 2; 	// skip the tag

			rx = QRegExp( ("</" + *itr + ".*>") );
			rx.setMinimal( true ); // make match non-greedy

			if( (endpos = rx.indexIn(str, startpos)) == -1 )
			{
				qDebug() << tr("+++++ starttag but no endtag found, FIXME");
				qDebug() << tr("+++++ line was: %1").arg(str);
				qDebug() << tr("+++++ searching for: %1").arg(*itr);
				continue; 		// not end tag found FIXME (endtag in one of the following lines)
			}

			fwd.addInfo( *itr, QString(str).midRef( startpos, endpos-startpos ).toString() );
			containsinfos = true;
		}
	}

	if( containsinfos )
	{
		m_fwds.push_back( fwd );
//		emit newRouterForwarding( this->getIP(), m_fwds );
	}
	else
	{
		qDebug() << tr("+++++ soap fwd reply didn't contain infos, reply was:");
		qDebug() << soapfwd;
	}
}


void UPnPRouter::addFwd ( const UPnPForward &fwd )	// activates port mapping
{
	QString query, action;
	QPointer<MiniGetter> getter = new MiniGetter();

	qDebug() << tr("\n+++++ Adding new Forward: \n");

	action = QString( "\"urn:schemas-upnp-org:service:%1:1#AddPortMapping\"" )
		.arg( m_servicetype );

	query = QString(
		"<?xml version=\"1.0\"?>\r\n"
		"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
		"<s:Body><u:AddPortMapping xmlns:u=\"urn:schemas-upnp-org:service:%9:1\">"
		"<NewRemoteHost>%1</NewRemoteHost>"
		"<NewExternalPort>%2</NewExternalPort>"
		"<NewProtocol>%3</NewProtocol>"
		"<NewInternalPort>%4</NewInternalPort>"
		"<NewInternalClient>%5</NewInternalClient>"
		"<NewEnabled>%6</NewEnabled>"
		"<NewPortMappingDescription>%7</NewPortMappingDescription>"
		"<NewLeaseDuration>%8</NewLeaseDuration>"
		"</u:AddPortMapping></s:Body></s:Envelope>")
		.arg( fwd.m_remoteHost )
		.arg( fwd.m_externalPort )
		.arg( fwd.m_protocol )
		.arg( fwd.m_internalPort )
		.arg( fwd.m_internalClient )
		.arg( fwd.m_enabled )
		.arg( fwd.m_description )
		.arg( fwd.m_leaseDuration )
		.arg( m_servicetype );
	
	connect( getter, SIGNAL(finished(QPointer<MiniGetter>)), this, SLOT(slot_addedFwd(QPointer<MiniGetter>)) );
	getter->soap( m_location, QUrl(m_controlurl), action, query, &m_forward_result );
}

void UPnPRouter::removeFwd ( const UPnPForward &fwd )	// removes port forwarding
{
	QString query, action;
	QPointer<MiniGetter> getter = new MiniGetter();

	qDebug() << tr("\n+++++ Removing Forward: \n");

	action = QString( "\"urn:schemas-upnp-org:service:%1:1#DeletePortMapping\"" )
		.arg( m_servicetype );

	query = QString(
		"<?xml version=\"1.0\"?>\r\n"
		"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
		"<s:Body><u:DeleteMapping xmlns:u=\"urn:schemas-upnp-org:service:%1:1\">"
		"<NewRemoteHost>%2</NewRemoteHost>"
		"<NewExternalPort>%3</NewExternalPort>"
		"<NewProtocol>%4</NewProtocol>"
		"</u:DeletePortMapping></s:Body></s:Envelope>")
		.arg( m_servicetype )
		.arg( fwd.m_remoteHost )
		.arg( fwd.m_externalPort )
		.arg( fwd.m_protocol );
	
	qDebug() << "+++++ removal query is:";
	qDebug() << query;
	connect( getter, SIGNAL(finished(QPointer<MiniGetter>)), this, SLOT(slot_removedFwd(QPointer<MiniGetter>)) );
	getter->soap( m_location, QUrl(m_controlurl), action, query, &m_removed_result );
}

void UPnPRouter::slot_addedFwd ( QPointer<MiniGetter> getter )
{
	if( ! getter )
		return;

	if( getter->status() == 200 )
	{
		m_currentForward = 0;
		downloadFwds();
	}
	else
	{
		QMessageBox::information( (QWidget*)NULL, tr("UPnP"), tr("Unable to add Port-Forwarding") );
	}

	getter->deleteLater();
}

void UPnPRouter::slot_removedFwd ( QPointer<MiniGetter> getter )
{
	if( ! getter )
		return;

	if( getter->status() == 200 )
	{
		m_currentForward = 0;
		downloadFwds();
	}
	else
	{
		QMessageBox::information( (QWidget*)NULL, tr("UPnP"), tr("Unable to remove Port-Forwarding") );
		qDebug() << "+++++ answer was:";
		qDebug() << m_removed_result;
	}

	getter->deleteLater();
}
