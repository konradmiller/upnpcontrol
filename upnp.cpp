#include "upnp.h"
#include "upnprouter.h"
#include <QUdpSocket>
#include <QStringList>
#include <QUrl>

#ifdef WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <errno.h>

	int * _p_errno()
	{
		static int Errno;
		return &Errno;
	}
	#define p_errno (*_p_errno())
#else
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <sys/types.h>

	#define p_errno errno
#endif

int _inet_aton( const char *cp, struct in_addr *addr )
{
#ifdef WIN32
	unsigned long addrl;
	addrl = inet_addr( cp );
	if( addrl != INADDR_NONE )
	{
		addr->S_un.S_addr = addrl;
		return 0;
	}
	else 
	{
		p_errno = EILSEQ;
		return -1;
	}
#else
	return inet_aton( cp, addr );
#endif
}


UPnP::UPnP ()
	: m_routers()	// std::vector<UPnPRouter*> m_routers;
{
	for( m_port=1900; m_port<1910; ++m_port )
	{
		if( ! bind(m_port) )
		{
			qDebug() << tr("+++++ Cannot bind to UDP port %1 : %2").arg(m_port).arg(errorString());
		}
		else
		{
			qDebug() << tr("+++++ Bound to UDP port %1").arg(m_port);
			break;
		}
	}

	connect( this, SIGNAL(readyRead()), this, SLOT(onReadyRead()) );
	connect( this, SIGNAL(error(QAbstractSocket::SocketError )), this, SLOT(error(QAbstractSocket::SocketError )) );

	join_UPnP_MulticastGroup();	
}


UPnP::~UPnP ()
{
	leave_UPnP_MulticastGroup();	

	// clean up upnp router list
	unsigned i = m_routers.size();
	while( i-- )
		m_routers[i]->deleteLater();
}


void UPnP::join_UPnP_MulticastGroup ()
{
	int fd = socketDescriptor(); 				// class UPnP : public QUdpSocket
	struct ip_mreq mreq;
	
	memset( &mreq, 0, sizeof(struct ip_mreq) );		// fills with zeros
	_inet_aton( "239.255.255.250", &mreq.imr_multiaddr );	// creates ip-binary-value from adress 		CAUTION: only linux -> should be changed to inet_addr()
	mreq.imr_interface.s_addr = htonl( INADDR_ANY );

	if( setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(struct ip_mreq)) < 0 )	// join multicast group
		qDebug() << tr("+++++ Failed to join multicast group 239.255.255.250");
	else
		qDebug() << tr("+++++ Joined multicast group 239.255.255.250");
}


void UPnP::leave_UPnP_MulticastGroup ()
{
	int fd = socketDescriptor();
	struct ip_mreq mreq;

	memset( &mreq, 0, sizeof(struct ip_mreq) );
	_inet_aton( "239.255.255.250", &mreq.imr_multiaddr );
	mreq.imr_interface.s_addr = htonl( INADDR_ANY );

	if( setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mreq, sizeof(struct ip_mreq)) < 0 )
		qDebug() << tr("+++++ Failed to leave multicast group 239.255.255.250");
	else
		qDebug() << tr("+++++ Left multicast group 239.255.255.250");
}


void UPnP::discover ()	// discovery via ssdp
{
	qDebug() << tr("+++++ Trying to find UPnP devices\n");

	const char* search_msg = "M-SEARCH * HTTP/1.1\r\n"
				 "HOST: 239.255.255.250:1900\r\n"
				 "ST:urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
				 "MAN:\"ssdp:discover\"\r\n"
				 "MX:3\r\n"
				 "\r\n\0";

	writeDatagram( search_msg, strlen(search_msg), QHostAddress("239.255.255.250"), 1900 );
}


void UPnP::error ( const QAbstractSocket::SocketError &err )
{
	Q_UNUSED( err );
	// Fixme
	qDebug() << tr("+++++ UPnP Error: %1").arg(errorString());
}


void UPnP::onReadyRead ()
{
	// Todo: 0 byte replies handled correctly?

	QByteArray data( pendingDatagramSize(), 0 ); // filled with zeros of corresponding size

	if( readDatagram(data.data(), pendingDatagramSize()) == -1 )
	{
		qDebug() << tr("+++++ Error Reading UDP Datagram: %1").arg(errorString());
		return;
	}

	UPnPRouter* r = parseResponse( data );

	if( r )
	{
		connect( r, SIGNAL(newRouterForwarding(const QString &, const std::vector<UPnPForward>&)), this, SIGNAL(newRouterForwarding(const QString &, const std::vector<UPnPForward>&)) );
		m_routers.push_back( r );
		emit newRouterDiscovered( m_routers.size() - 1 );
	}
}


UPnPRouter* UPnP::parseResponse ( const QByteArray &msg )
{
	QString server, location, line;
	QStringList lines = QString::fromAscii(msg).split("\r\n");

	// first read first line and see if contains a HTTP 200 OK message
	line = lines.first();

	if( line.contains("M-SEARCH") ) // ignore M-SEARCH
	{
//		qDebug() << tr("+++++ Contains M-SEARCH - ignoring");
		return NULL;
	}

//	qDebug() << tr("\n============ i got this router ==============");
//	qDebug() << msg;
//	qDebug() << tr("============ over and out  ==============\n");


	if( ! line.contains("HTTP") )
		if( (! line.contains("NOTIFY")) && (! line.contains("200")) ) 	// it is either a 200 OK or a NOTIFY
			return NULL;						// or useless ;-)

	// quick check that the response being parsed is valid
	bool validDevice = false;
	for( int i=0; (i < lines.count()) && (! validDevice); ++i )
	{
		line = lines[i];
		if( (line.contains("ST:") || line.contains("NT:"))  &&  line.contains("InternetGatewayDevice") )
			validDevice = true;
	}

	if( ! validDevice )
		return NULL;


	// read all lines and try to find the server and location fields
	for( int i=1; i < lines.count(); ++i )
	{
		line = lines[i];
		if( line.startsWith("Location") || line.startsWith("LOCATION") || line.startsWith("location") )
		{
			location = line.mid(line.indexOf(':') + 1).trimmed();

			if( ! QUrl(location).isValid() )
				return NULL;
		}
		else if( line.startsWith("Server") || line.startsWith("server") || line.startsWith("SERVER") )
		{
			server = line.mid(line.indexOf(':') + 1).trimmed();
			if( server.length() == 0 )
				return NULL;
		}
	}

	if( (*this)[QUrl(location).host()] == NULL ) // new one :)
		return new UPnPRouter( server, location );
	else
		return NULL;
}

void UPnP::addForward ( const QString &ip, const UPnPForward &fwd )
{
	UPnPRouter* r = (*this)[ ip ];
	if( r )
		r->addFwd( fwd );
	else
		qDebug() << tr("+++++ No router with IP %1 found!").arg(ip);
}

void UPnP::removeForward( const QString &ip, const UPnPForward &fwd)
{
         UPnPRouter* r = (*this)[ ip ];
         if( r )
                 r->removeFwd( fwd );
         else
                 qDebug() << tr("+++++ No router with IP %1 found!").arg(ip);
}

unsigned UPnP::size ()
{
	return m_routers.size();
}

UPnPRouter* UPnP::operator[] ( int i )
{
	if( (m_routers.size() > (unsigned) i) && (i >= 0) )
		return m_routers[i];
	else
		return NULL;
}

UPnPRouter* UPnP::operator[] ( const QString &ip )
{
	int pos = 0;
	for( std::vector<UPnPRouter*>::iterator itr = m_routers.begin(); itr != m_routers.end(); ++itr )
	{
		if( ((*itr)->getIP()) == ip )
			return m_routers[pos];
		pos++;
	}

	return NULL;
}
