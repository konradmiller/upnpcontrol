#ifndef UPNP_H
#define UPNP_H

#include <vector>
#include <QUdpSocket>
#include <stdint.h>

class UPnPRouter;
class QUrl;

struct UPnPForward
{
	QString  m_remoteHost, m_externalPort, m_protocol, m_internalPort, m_internalClient, m_enabled, m_description, m_leaseDuration;

	void addInfo( QString type, QString value )
	{
		if     ( type == "NewRemoteHost" 	     ) m_remoteHost 	= value;
		else if( type == "NewExternalPort" 	     ) m_externalPort 	= value;
		else if( type == "NewProtocol" 		     ) m_protocol 	= value;
		else if( type == "NewInternalPort" 	     ) m_internalPort 	= value;
		else if( type == "NewInternalClient" 	     ) m_internalClient = value;
		else if( type == "NewEnabled" 		     ) m_enabled 	= value;
		else if( type == "NewPortMappingDescription" ) m_description 	= value;
		else if( type == "NewLeaseDuration" 	     ) m_leaseDuration 	= value;
	}
};


class UPnP : public QUdpSocket
{
	Q_OBJECT

public:
	UPnP();
	virtual ~UPnP();

	void    	addForward ( const QString &ip, const UPnPForward &fwd );
	void 		removeForward( const QString & ip, const UPnPForward &fwd );
	unsigned 	size ();
	UPnPRouter* 	operator[] ( int i );
	UPnPRouter* 	operator[] ( const QString &ip );

public slots:
	void discover ();

private slots:
	void error ( const QAbstractSocket::SocketError &err );
	void onReadyRead ();

signals:
	void newRouterDiscovered ( int num );
	void newRouterForwarding ( const QString &, const std::vector<UPnPForward> & );

private:
	void join_UPnP_MulticastGroup  ();
	void leave_UPnP_MulticastGroup ();
	UPnPRouter* parseResponse ( const QByteArray &msg );

	std::vector<UPnPRouter*> m_routers;
	uint16_t m_port;
};

#endif
