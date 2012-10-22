#ifndef NETWORK_PROXY_AUTO_CONFIG_H_7E83DFB1_54E9_4DAA_BAF5_8B9D79933814
#define NETWORK_PROXY_AUTO_CONFIG_H_7E83DFB1_54E9_4DAA_BAF5_8B9D79933814

#include <QtNetwork/QNetworkAccessManager>
#include <QNetworkProxyQuery>
#include <QtCore/QUrl>
#include <QWebPage>
#include <QVariant>

class NetworkProxyAutoConfigDownloader : public QObject
{
    Q_OBJECT
public:
    NetworkProxyAutoConfigDownloader();
    ~NetworkProxyAutoConfigDownloader();

    /**
     * fetches a resource
     * @param  url      URL
     * @param  timeout  timeout value in milliseconds
     */
    QString get(QUrl const & url, unsigned int timeout);

private:
    QNetworkAccessManager* manager_;
};

class NetworkProxyAutoConfig : public QObject
{
    Q_OBJECT
public:
    /**
     *  constructor
     *  @param  script      proxy auto config script (contents, not path)
     */
    NetworkProxyAutoConfig(QString const & script);
    ~NetworkProxyAutoConfig();

    /**
     *  @brief: runs the proxy auto config file
     */
    QString eval(QString const & url, QString const & host);
public slots:
    bool isInNet(QVariantList args);
    bool isPlainHostName(const QString &host);
    QString dnsResolve(const QString &host);
    QString myIpAddress();
    bool shExpMatch(const QString &str, const QString &shexp);
    bool dnsDomainIs(const QString &host, const QString &domain);
    bool localHostOrDomainIs(const QString &host, const QString &hostdom);
    bool isResolvable(const QString &host);

private slots:
    void onCreateObjects();

private:
    QString pacScript_;
    QWebPage* page_;
};
      
class NetworkProxyAutoConfigFactory : public QNetworkProxyFactory
{
public:
    NetworkProxyAutoConfigFactory();
    ~NetworkProxyAutoConfigFactory();

    void setProxyAutoConfig(QString const & url);
    virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery & query);

    void setUser(QString const & s)
    {
        user_ = s;
    }

    QString const & user() const
    {
        return user_;
    }

    void setPassword(QString const & s)
    {
        password_ = s;
    }

    QString const & password() const
    {
        return password_;
    }

private:
    NetworkProxyAutoConfig* pac_;
    QString user_;
    QString password_;
};

#endif // NETWORK_PROXY_AUTO_CONFIG_H_7E83DFB1_54E9_4DAA_BAF5_8B9D79933814

