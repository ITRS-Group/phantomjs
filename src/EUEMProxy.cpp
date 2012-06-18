#include "EUEMProxy.h"

#include <iostream>
#include <QTimer>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWebFrame>
#include <QTextStream>
#include <QHostInfo>
#include <QNetworkInterface>

#include <cassert>

////////////////////////////////////////////////////////////////////////////////
//                               EUEMDownloader
////////////////////////////////////////////////////////////////////////////////

EUEMDownloader::EUEMDownloader()
  : manager_( 0 )
{
    manager_ = new QNetworkAccessManager(this);
}

EUEMDownloader::~EUEMDownloader()
{
    delete manager_;
}

QString EUEMDownloader::get(QUrl const & url, unsigned int timeout )
{
    QNetworkRequest const request(url);
    QEventLoop loop;

    QObject::connect(manager_, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()));
    QTimer::singleShot(timeout, &loop, SLOT(quit()));

    QNetworkReply* reply = manager_->get(request);
    loop.exec();

    QString const replyString = QString::fromUtf8(reply->readAll().data());
    reply->deleteLater();
    return replyString;
}

////////////////////////////////////////////////////////////////////////////////
//                                  EUEMPac
////////////////////////////////////////////////////////////////////////////////

EUEMPac::EUEMPac(QString const & script)
  : pacScript_(script)
  , page_(new QWebPage())
{
    connect( page_->mainFrame(),
             SIGNAL(javaScriptWindowObjectCleared()),
             this,
             SLOT(onCreateObjects()));
}

EUEMPac::~EUEMPac()
{
    page_->deleteLater();
}

void EUEMPac::onCreateObjects()
{
    QWebFrame * const frame = page_->mainFrame();
    frame->addToJavaScriptWindowObject("autoproxy", this);

    QString callbacks;
    callbacks.append("function isInNet(){ return autoproxy.isInNet(Array.prototype.slice.call(arguments)); }");
    callbacks.append("function isPlainHostName(hostname) { return autoproxy.isPlainHostName(hostname); }");
    callbacks.append("function dnsResolve(hostname) { return autoproxy.dnsResolve(hostname); }");
    callbacks.append("function myIpAddress() { return autoproxy.myIpAddress(); }");
    callbacks.append("function shExpMatch(str, shexp) { return autoproxy.shExpMatch(str, shexp); }");
    callbacks.append("function dnsDomainIs(host, domain) { return autoproxy.dnsDomainIs(host, domain); }");
    callbacks.append("function localHostOrDomainIs(host, hostdom) { return autoproxy.localHostOrDomainIs(host, hostdom); }");
    callbacks.append("function isResolvable(host) { return autoproxy.isResolvable(host); }");
    callbacks.append(pacScript_);

    frame->evaluateJavaScript(callbacks);
}

QString EUEMPac::eval(QString const & url, QString const & host)
{
    QString cmd;
    QTextStream(&cmd) << "FindProxyForURL('" << url << "','" << host << "');";
    return page_->mainFrame()->evaluateJavaScript(cmd).toString();
}

bool EUEMPac::isInNet(QVariantList args)
{
    QString host = args[0].toString();
    QHostInfo info = QHostInfo::fromName(host);
    if( info.error() == QHostInfo::NoError )
    {
        QHostAddress const hostAddress = info.addresses().first();
        QString const pattern = args[1].toString();
        QString const mask = args[2].toString();
        QPair<QHostAddress, int> const subnet = QHostAddress::parseSubnet(QString("%1/%2").arg(pattern,mask));
        return hostAddress.isInSubnet(subnet);
    }
    else
    {
        return false;
    }
}

bool EUEMPac::isPlainHostName(const QString &host)
{
    return !host.contains(".");
}

QString EUEMPac::dnsResolve(const QString &host)
{
    QHostInfo info = QHostInfo::fromName( host );
    QList<QHostAddress> addresses = info.addresses();

    if( addresses.isEmpty() )
    {
        return NULL;
    }

    return addresses.first().toString();
}

QString EUEMPac::myIpAddress()
{
    foreach ( QHostAddress address, QNetworkInterface::allAddresses() )
    {
        if( !address.isNull()
            && address != QHostAddress::Null
            && address != QHostAddress::LocalHost
            && address != QHostAddress::LocalHostIPv6
            && address != QHostAddress::Broadcast
            && address != QHostAddress::Any
            && address != QHostAddress::AnyIPv6)
        {
            return address.toString();
        }
    }

    return NULL;
}

bool EUEMPac::shExpMatch(const QString &str, const QString &shexp)
{
    QRegExp re( shexp, Qt::CaseSensitive, QRegExp::Wildcard );
    return re.exactMatch( str );
}

bool EUEMPac::dnsDomainIs(const QString &host, const QString &domain)
{
    return host.endsWith( domain );
}

bool EUEMPac::localHostOrDomainIs(const QString &host, const QString &hostdom)
{
    // Check for Exact Match
    if( host == hostdom )
    {
        return true;
    }

    // Check for Hostname match; domain not specified
    return hostdom.startsWith( host + "." );
}

bool EUEMPac::isResolvable(const QString &host)
{
    return dnsResolve(host) != NULL;
}

EUEMProxyFactory::EUEMProxyFactory()
  : pac_( 0 )
{
}

EUEMProxyFactory::~EUEMProxyFactory()
{
    delete pac_;
}

void EUEMProxyFactory::setProxyAutoConfig(QString const & url)
{
    EUEMDownloader dl;

    // 10-second timeout for downloads, in case we're given the wrong url
    //
    QString script = dl.get(QUrl(url), 10 * 1000).trimmed();
    if( !script.isEmpty() )
    {
        EUEMPac* tmp( new EUEMPac( script ));
        std::swap(pac_, tmp);
        delete tmp;
    }
}

static void addProxy(
    QList<QNetworkProxy>&       proxyList,
    QNetworkProxy::ProxyType    type,
    QString const &             str,
    QString const &             user,
    QString const &             password )
{
    int const delim = str.indexOf(":");
    QString host;
    quint16 port;
    if( delim == -1 )
    {
        host = str;
        port = 8080;
    }
    else
    {
        host = str.left(delim).trimmed();
        port = str.mid(delim + 1).trimmed().toUInt();
    }
    proxyList.append( QNetworkProxy( type, host, port, user, password ));
}

QList<QNetworkProxy> EUEMProxyFactory::queryProxy(const QNetworkProxyQuery& query)
{
    QList<QNetworkProxy> proxyList;
    if( pac_ )
    {
        QStringList const proxies =
            pac_->eval(query.url().toString(), query.peerHostName()).split(";");

        QStringList::const_iterator end = proxies.end();
        for( QStringList::const_iterator itor = proxies.begin(); itor != end; ++itor )
        {
            QString const proxy = itor->trimmed();
            if( proxy.isEmpty() )
            {
                continue;
            }

            if( proxy == "DIRECT" )
            {
                proxyList.append(QNetworkProxy::NoProxy);
            }
            else if( proxy.startsWith( "PROXY " ))
            {
                addProxy( proxyList, QNetworkProxy::HttpProxy, proxy.mid(6).trimmed(), user_, password_ );
            }
            else if( proxy.startsWith( "SOCKS " ))
            {
                addProxy( proxyList, QNetworkProxy::HttpProxy, proxy.mid(6).trimmed(), user_, password_ );
            }
        }
    }
    else
    {
        proxyList.append(QNetworkProxy::NoProxy);
    }
    return proxyList;
}
