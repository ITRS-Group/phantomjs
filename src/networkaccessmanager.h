/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2011 Ivan De Marino <ivan.de.marino@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef NETWORKACCESSMANAGER_H
#define NETWORKACCESSMANAGER_H

#include <QAuthenticator>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QSslConfiguration>

class Config;
class QNetworkDiskCache;

class NetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    NetworkAccessManager(QObject *parent, const Config *config);
    void setUserName(const QString &userName);
    void setPassword(const QString &password);
    void setCustomHeaders(const QVariantMap &headers);
    QVariantMap customHeaders() const;

    void setCookieJar(QNetworkCookieJar *cookieJar);

protected:
    bool m_ignoreSslErrors;
    QString m_userName;
    QString m_password;
    QNetworkReply *createRequest(Operation op, const QNetworkRequest & req, QIODevice * outgoingData = 0);

signals:
    void resourceRequested(const QVariant& data);
    void resourceReceived(const QVariant& data);

private slots:
    void handleStarted();
    void handleFinished(QNetworkReply *reply);
    void provideAuthentication(QNetworkReply *reply, QAuthenticator *authenticator);
    void handleSslErrors(QNetworkReply* reply, const QList<QSslError> &errors);

private:
    QHash<QNetworkReply*, int> m_ids;
    QSet<QNetworkReply*> m_started;
    int m_idCounter;
    QNetworkDiskCache* m_networkDiskCache;
    QVariantMap m_customHeaders;
    QSslConfiguration m_sslConfiguration;
};

#endif // NETWORKACCESSMANAGER_H
