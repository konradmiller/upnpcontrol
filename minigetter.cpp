#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QPointer>
#include "minigetter.h"


MiniGetter::MiniGetter ()
	: m_status( -1 )
	, m_request()
	, m_temp_result()
	, m_result( NULL )
{
	connect( this, SIGNAL(connected()),    this, SLOT(slot_connected()) );
	connect( this, SIGNAL(readyRead()),    this, SLOT(slot_readyRead()) );
	connect( this, SIGNAL(disconnected()), this, SLOT(slot_done()) );
	connect( this, SIGNAL(error(QAbstractSocket::SocketError )), this, SLOT(error(QAbstractSocket::SocketError )) );
}


void MiniGetter::error ( const QAbstractSocket::SocketError &err )
{
	if( err != QAbstractSocket::RemoteHostClosedError )
		qDebug() << tr("+++++ MINIGETTER Error: %1").arg(errorString());
}


void MiniGetter::wget ( const QUrl &url, QByteArray *result )
{
	result->clear();
	m_result  = result;

        m_request = QString(
		"GET " + url.encodedPath() + " HTTP/1.1\r\n"
		"Host: " + url.host() + ":%1\r\n"
		"Connection: Close\r\n" )
		.arg( url.port() );

//	qDebug() << tr("+++++ Minigetter.wget( %1 : %2 )").arg(url.host()).arg( url.port());
	connectToHost( url.host(), url.port() );
}


void MiniGetter::soap ( const QUrl &url, const QUrl &posturl, const QString &soapaction, const QString &request, QByteArray *result )
{
	result->clear();
	m_result = result;

	QString header = QString(
		"POST %1 HTTP/1.1\r\n"
		"HOST: %2:%3\r\n"
		"Content-length: %4\r\n"
		"Content-Type: text/xml\r\n"
		"Connection: Close\r\n"
		"SOAPAction: %5\r\n\r\n" )
		.arg( posturl.toString() )
		.arg( url.host() )
		.arg( url.port() )
		.arg( request.length() )
		.arg( soapaction );
	
	m_request = header + request;
//	qDebug() << tr("+++++ REQUEST:\n%1").arg(m_request);

	// send query
//	qDebug() << tr("+++++ Minigetter.soap( %1 : %2 )").arg(url.host()).arg(url.port());
	connectToHost( url.host(), url.port() );
}


void MiniGetter::slot_connected ()
{
//	qDebug() << tr("+++++ %1").arg(m_request);

        // send query
        QTextStream out( this );
        out << m_request << "\r\n";
	out.flush();
}


void MiniGetter::slot_readyRead ()
{
	QTextStream in( this );
	m_temp_result += in.readAll();
//	qDebug() << tr("++++ readyRead()");

	if(m_temp_result.contains("</s:Envelope>"))
		disconnectFromHost();
}


void MiniGetter::slot_done ()
{
	int i;
	QStringList lines = m_temp_result.split( "\r\n" );
	if( (i=lines.indexOf("")) != -1 )  // matches end of header
		for( ; i < lines.size(); ++i )
			(*m_result) += lines[i];
	
//	qDebug() << tr("+++++ soap-response:\n%1").arg(lines);

	int statuscode = 417; // Expectation Failed
	if( (i=lines.indexOf(QRegExp("HTTP/.*"))) != -1 )
		sscanf( lines[i].toAscii(), "%*s %d %*s", &statuscode ); 	
	else
		qDebug() << tr("+++++ no status code line found - returning 417");

	m_status = statuscode;
	emit finished( this );
}

int MiniGetter::status()
{
	return m_status;
}
