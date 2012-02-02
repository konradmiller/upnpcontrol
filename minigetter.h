#ifndef MINIGETTER_H
#define MINIGETTER_H

#include <QUrl>
#include <QByteArray>
#include <QTcpSocket>
#include <QPointer>

class MiniGetter : public QTcpSocket
{
	Q_OBJECT

public:
	MiniGetter();
	void get   ( const QUrl &url );
	void wget  ( const QUrl &url, QByteArray *result );
	void soap  ( const QUrl &url, const QUrl &posturl, const QString &soapaction, const QString &request, QByteArray *result );
	int status ();

private slots:
	void slot_connected ();
	void slot_readyRead ();
	void slot_done ();
	void error ( const QAbstractSocket::SocketError &err );

signals:
	void finished ( QPointer<MiniGetter> );

private:
	int         m_status;
	QString     m_request;
	QString     m_temp_result;
	QByteArray *m_result;
};

#endif
