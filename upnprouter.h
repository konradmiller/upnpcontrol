#ifndef UPNPROUTER_H
#define UPNPROUTER_H

#include <vector>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QTcpSocket>
#include <QPointer>
#include "upnp.h"

class MiniGetter;

class UPnPRouter : public QTcpSocket // public QObject
{
	Q_OBJECT

private:
	QString     	  	 m_server,
				 m_controlurl,
				 m_servicetype;
	QUrl        		 m_location;
	QByteArray  		 m_xmldescription;
	std::vector<UPnPForward> m_fwds;
	QNetworkAccessManager 	 *m_networkaccessmanager;
	int                      m_currentForward;
	QByteArray 		 m_forward_result;
	QByteArray 		 m_removed_result;

	void 	parseXmlFile ();
	void 	parseFwd ( const QByteArray &soapfwd );


public:
	UPnPRouter ( const QString &server, const QString &xml_location );

	// we need copy-constructor and operator= so the xml file is only fetched/parsed once
	UPnPRouter ( const UPnPRouter &r );
	UPnPRouter& operator= ( const UPnPRouter &r );

	QUrl        getDescriptionUrl ();
	QString     getIP ();
	QString     getControlUrl ();
	unsigned    size ();
	UPnPForward getForward ( unsigned n );
	void 	    downloadFwds ();
	void        addFwd ( const UPnPForward &fwd );
	void        removeFwd( const UPnPForward &fwd );
	void        downloadXmlFile ();


signals:
	void newRouterForwarding ( const QString &, const std::vector<UPnPForward> & );
	
private slots:
	void slot_downloadedXmlFile ( QPointer<MiniGetter> getter );
	void slot_downloadedFwd ( QPointer<MiniGetter> getter );
	void slot_addedFwd ( QPointer<MiniGetter> getter );
	void slot_removedFwd ( QPointer<MiniGetter> getter );

};

#endif
