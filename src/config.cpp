/*
  This file is part of the PhantomJS project from Ofi Labs.

  Copyright (C) 2012 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2011 execjosh, http://execjosh.blogspot.com

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

#include "config.h"

#include <QDir>
#include <QWebPage>
#include <QWebFrame>
#include <QNetworkProxy>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslError>

#include "terminal.h"
#include "qcommandline.h"
#include "utils.h"

#include <iostream>

static const struct QCommandLineConfigEntry flags[] =
{
    { QCommandLine::Option, '\0', "cookies-file", "Sets the file name to store the persistent cookies", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "config", "Specifies JSON-formatted configuration file", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "debug", "Prints additional warning and debug message: 'yes' or 'no' (default)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "disk-cache", "Enables disk cache: 'yes' (default) or 'no'", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "ignore-ssl-errors", "Ignores SSL errors (expired/self-signed certificate errors): 'yes' or 'no' (default)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "load-images", "Loads all inlined images: 'yes' (default) or 'no'", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "local-storage-path", "Specifies the location for offline local storage", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "local-storage-quota", "Sets the maximum size of the offline local storage (in KB)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "local-to-remote-url-access", "Allows local content to access remote URL: 'yes' or 'no' (default)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "max-disk-cache-size", "Limits the size of the disk cache (in KB)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "output-encoding", "Sets the encoding for the terminal output, default is 'utf8'", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "remote-debugger-port", "Starts the script in a debug harness and listens on the specified port", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "remote-debugger-autorun", "Runs the script in the debugger immediately: 'yes' or 'no' (default)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "proxy", "Sets the proxy server, e.g. '--proxy=http://proxy.company.com:8080'", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "proxy-auth", "Provides authentication information for the proxy, e.g. ''-proxy-auth=username:password'", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "proxy-type", "Specifies the proxy type, 'http' (default), 'none' (disable completely), or 'socks5'", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "script-encoding", "Sets the encoding used for the starting script, default is 'utf8'", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "web-security", "Enables web security, 'yes' (default) or 'no'", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "pac", "Sets the auto-config proxy url, e.g. '--pac=http://proxy.company.com/autoproxy.pac'", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "cert-authorities-path", "Loads CA Root certificates from the location specified", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "local-certificate-file", "Sets Personal Certificate File (PKCS 12 Format)", QCommandLine::Optional },
    { QCommandLine::Option, '\0', "local-certificate-passphrase", "Sets the Personal Certificate Pass Phrase", QCommandLine::Optional },
    { QCommandLine::Param, '\0', "script", "Script", QCommandLine::Flags(QCommandLine::Optional|QCommandLine::ParameterFence)},
    { QCommandLine::Param, '\0', "argument", "Script argument", QCommandLine::OptionalMultiple },
    { QCommandLine::Switch, 'h', "help", "Shows this message and quits", QCommandLine::Optional },
    { QCommandLine::Switch, 'v', "version", "Prints out PhantomJS version", QCommandLine::Optional },
    QCOMMANDLINE_CONFIG_ENTRY_END
};

Config::Config(QObject *parent)
    : QObject(parent)
{
    m_cmdLine = new QCommandLine;

    // We will handle --help and --version ourselves in phantom.cpp
    m_cmdLine->enableHelp(false);
    m_cmdLine->enableVersion(false);

    resetToDefaults();
}

void Config::init(const QStringList *const args)
{
    resetToDefaults();

    processArgs(*args);
}

void Config::processArgs(const QStringList &args)
{
    connect(m_cmdLine, SIGNAL(switchFound(const QString &)), this, SLOT(handleSwitch(const QString &)));
    connect(m_cmdLine, SIGNAL(optionFound(const QString &, const QVariant &)), this, SLOT(handleOption(const QString &, const QVariant &)));
    connect(m_cmdLine, SIGNAL(paramFound(const QString &, const QVariant &)), this, SLOT(handleParam(const QString &, const QVariant &)));
    connect(m_cmdLine, SIGNAL(parseError(const QString &)), this, SLOT(handleError(const QString &)));

    m_cmdLine->setArguments(args);
    m_cmdLine->setConfig(flags);
    m_cmdLine->parse();
}

void Config::loadJsonFile(const QString &filePath)
{
    QString jsonConfig;
    QFile f(filePath);

    // Check file exists and is readable
    if (!f.exists() || !f.open(QFile::ReadOnly | QFile::Text)) {
        Terminal::instance()->cerr("Unable to open config: \"" + filePath + "\"");
        return;
    }

    // Read content
    jsonConfig = QString::fromUtf8(f.readAll().trimmed());
    f.close();

    // Check it's a valid JSON format
    if (jsonConfig.isEmpty() || !jsonConfig.startsWith('{') || !jsonConfig.endsWith('}')) {
        Terminal::instance()->cerr("Config file MUST be in JSON format!");
        return;
    }

    // Load configurator
    QString configurator = Utils::readResourceFileUtf8(":/configurator.js");

    // Use a temporary QWebPage to load the JSON configuration in this Object using the 'configurator' above
    QWebPage webPage;
    // Add this object to the global scope
    webPage.mainFrame()->addToJavaScriptWindowObject("config", this);
    // Apply the JSON config settings to this very object
    webPage.mainFrame()->evaluateJavaScript(configurator.arg(jsonConfig), QString());
}

QString Config::helpText() const
{
    return m_cmdLine->help();
}

bool Config::autoLoadImages() const
{
    return m_autoLoadImages;
}

void Config::setAutoLoadImages(const bool value)
{
    m_autoLoadImages = value;
}

QString Config::cookiesFile() const
{
    return m_cookiesFile;
}

void Config::setCookiesFile(const QString &value)
{
    m_cookiesFile = value;
}

QString Config::offlineStoragePath() const
{
    return m_offlineStoragePath;
}

void Config::setOfflineStoragePath(const QString &value)
{
    QDir dir(value);
    m_offlineStoragePath = dir.absolutePath();
}

int Config::offlineStorageDefaultQuota() const
{
    return m_offlineStorageDefaultQuota;
}

void Config::setOfflineStorageDefaultQuota(int offlineStorageDefaultQuota)
{
    m_offlineStorageDefaultQuota = offlineStorageDefaultQuota * 1024;
}

bool Config::diskCacheEnabled() const
{
    return m_diskCacheEnabled;
}

void Config::setDiskCacheEnabled(const bool value)
{
    m_diskCacheEnabled = value;
}

int Config::maxDiskCacheSize() const
{
    return m_maxDiskCacheSize;
}

void Config::setMaxDiskCacheSize(int maxDiskCacheSize)
{
    m_maxDiskCacheSize = maxDiskCacheSize;
}

bool Config::ignoreSslErrors() const
{
    return m_ignoreSslErrors;
}

void Config::setIgnoreSslErrors(const bool value)
{
    m_ignoreSslErrors = value;
}

bool Config::localToRemoteUrlAccessEnabled() const
{
    return m_localToRemoteUrlAccessEnabled;
}

void Config::setLocalToRemoteUrlAccessEnabled(const bool value)
{
    m_localToRemoteUrlAccessEnabled = value;
}

QString Config::outputEncoding() const
{
    return m_outputEncoding;
}

void Config::setOutputEncoding(const QString &value)
{
    if (value.isEmpty()) {
        return;
    }

    m_outputEncoding = value;
}

QString Config::proxyType() const
{
    return m_proxyType;
}

void Config::setProxyType(const QString value)
{
    m_proxyType = value;
}

QString Config::proxy() const
{
    return proxyHost() + ":" + proxyPort();
}

void Config::setProxy(const QString &value)
{
    QString proxyHost = value;
    int proxyPort = 1080;

    if (proxyHost.lastIndexOf(':') > 0) {
        bool ok = true;
        int port = proxyHost.mid(proxyHost.lastIndexOf(':') + 1).toInt(&ok);
        if (ok) {
            proxyHost = proxyHost.left(proxyHost.lastIndexOf(':')).trimmed();
            proxyPort = port;
        }
    }

    setProxyHost(proxyHost);
    setProxyPort(proxyPort);
}

void Config::setProxyAuth(const QString &value)
{
    QString proxyUser = value;
    QString proxyPass = "";

    if (proxyUser.lastIndexOf(':') > 0) {
        proxyPass = proxyUser.mid(proxyUser.lastIndexOf(':') + 1).trimmed();
        proxyUser = proxyUser.left(proxyUser.lastIndexOf(':')).trimmed();

        setProxyAuthUser(proxyUser);
        setProxyAuthPass(proxyPass);
    }
}

QString Config::proxyAuth() const
{
    return proxyAuthUser() + ":" + proxyAuthPass();
}

QString Config::proxyAuthUser() const
{
    return m_proxyAuthUser;
}

QString Config::proxyAuthPass() const
{
    return m_proxyAuthPass;
}

QString Config::proxyHost() const
{
    return m_proxyHost;
}

int Config::proxyPort() const
{
    return m_proxyPort;
}

QStringList Config::scriptArgs() const
{
    return m_scriptArgs;
}

void Config::setScriptArgs(const QStringList &value)
{
    m_scriptArgs.clear();

    QStringListIterator it(value);
    while (it.hasNext()) {
        m_scriptArgs.append(it.next());
    }
}

QString Config::scriptEncoding() const
{
    return m_scriptEncoding;
}

void Config::setScriptEncoding(const QString &value)
{
    if (value.isEmpty()) {
        return;
    }

    m_scriptEncoding = value;
}

QString Config::scriptFile() const
{
    return m_scriptFile;
}

void Config::setScriptFile(const QString &value)
{
    m_scriptFile = value;
}

QString Config::unknownOption() const
{
    return m_unknownOption;
}

void Config::setUnknownOption(const QString &value)
{
    m_unknownOption = value;
}

bool Config::versionFlag() const
{
    return m_versionFlag;
}

void Config::setVersionFlag(const bool value)
{
    m_versionFlag = value;
}

bool Config::debug() const
{
    return m_debug;
}

void Config::setDebug(const bool value)
{
    m_debug = value;
}

int Config::remoteDebugPort() const
{
    return m_remoteDebugPort;
}

void Config::setRemoteDebugPort(const int port)
{
    m_remoteDebugPort = port;
}

bool Config::remoteDebugAutorun() const
{
    return m_remoteDebugAutorun;
}

void Config::setRemoteDebugAutorun(const bool value)
{
    m_remoteDebugAutorun = value;
}

bool Config::webSecurityEnabled() const
{
    return m_webSecurityEnabled;
}

void Config::setWebSecurityEnabled(const bool value)
{
    m_webSecurityEnabled = value;
}

void Config::setJavascriptCanOpenWindows(const bool value)
{
    m_javascriptCanOpenWindows = value;
}

bool Config::javascriptCanOpenWindows() const
{
    return m_javascriptCanOpenWindows;
}

void Config::setJavascriptCanCloseWindows(const bool value)
{
    m_javascriptCanCloseWindows = value;
}

bool Config::javascriptCanCloseWindows() const
{
    return m_javascriptCanCloseWindows;
}

// private:
void Config::resetToDefaults()
{
    m_autoLoadImages = true;
    m_cookiesFile = QString();
    m_offlineStoragePath = QString();
    m_offlineStorageDefaultQuota = -1;
    m_diskCacheEnabled = false;
    m_maxDiskCacheSize = -1;
    m_ignoreSslErrors = false;
    m_localToRemoteUrlAccessEnabled = false;
    m_outputEncoding = "UTF-8";
    m_proxyType = "http";
    m_proxyHost.clear();
    m_proxyPort = 1080;
    m_proxyAuthUser.clear();
    m_proxyAuthPass.clear();
    m_scriptArgs.clear();
    m_scriptEncoding = "UTF-8";
    m_scriptFile.clear();
    m_unknownOption.clear();
    m_versionFlag = false;
    m_debug = false;
    m_remoteDebugPort = -1;
    m_remoteDebugAutorun = false;
    m_webSecurityEnabled = true;
    m_javascriptCanOpenWindows = true;
    m_javascriptCanCloseWindows = true;
    m_helpFlag = false;
    m_printDebugMessages = false;
}

void Config::setProxyAuthPass(const QString &value)
{
    m_proxyAuthPass = value;
}

void Config::setProxyAuthUser(const QString &value)
{
    m_proxyAuthUser = value;
}

void Config::setProxyHost(const QString &value)
{
    m_proxyHost = value;
}

void Config::setProxyPort(const int value)
{
    m_proxyPort = value;
}

bool Config::helpFlag() const
{
    return m_helpFlag;
}

void Config::setHelpFlag(const bool value)
{
    m_helpFlag = value;
}

bool Config::printDebugMessages() const
{
    return m_printDebugMessages;
}

void Config::setPrintDebugMessages(const bool value)
{
    m_printDebugMessages = value;
}

void Config::setProxyAutoConfig(const QString& value)
{
    m_proxyAutoConfig = value;
}

QString Config::proxyAutoConfig() const
{
    return m_proxyAutoConfig;
}

QString Config::certAuthoritiesPath() const
{
    return m_certAuthoritiesPath;
}

void Config::setCertAuthoritiesPath(const QString &dirPath)
{
    m_certAuthoritiesPath = dirPath.trimmed();

    if(!m_certAuthoritiesPath.isEmpty())
    {
        QList<QSslCertificate> cacerts;
        QDir dir(m_certAuthoritiesPath);

        if (dir.exists()) 
        {
            QDir::Filters dirFilter = QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::AllDirs | QDir::Files;
            foreach(QFileInfo info, dir.entryInfoList(dirFilter, QDir::DirsFirst)) 
            {
                cacerts += QSslCertificate::fromPath(info.absoluteFilePath());
            }
            QSslSocket::setDefaultCaCertificates(cacerts);
        }
    }
    else
    {
        Terminal::instance()->cerr("Unable to load certificates.");
    }
}

void Config::setLocalCertificateFile(const QString &value)
{
    m_localCertFile = value;
}

QString Config::localCertificateFile() const
{
    return m_localCertFile;
}

void Config::setLocalCertificatePassPhrase(const QString &value)
{
    m_localCertPassPhrase = value;
}

QString Config::localCertificatePassPhrase() const
{
    return m_localCertPassPhrase;
}

void Config::handleSwitch(const QString &sw)
{
    setHelpFlag(sw == "help");
    setVersionFlag(sw == "version");
}

void Config::handleOption(const QString &option, const QVariant &value)
{
    bool boolValue = false;

    QStringList booleanFlags;
    booleanFlags << "debug";
    booleanFlags << "disk-cache";
    booleanFlags << "ignore-ssl-errors";
    booleanFlags << "load-images";
    booleanFlags << "local-to-remote-url-access";
    booleanFlags << "remote-debugger-autorun";
    booleanFlags << "web-security";
    if (booleanFlags.contains(option)) {
        if ((value != "true") && (value != "yes") && (value != "false") && (value != "no")) {
            setUnknownOption(QString("Invalid values for '%1' option.").arg(option));
            return;
        }
        boolValue = (value == "true") || (value == "yes");
    }

    if (option == "cookies-file") {
        setCookiesFile(value.toString());
    }

    if (option == "config") {
        loadJsonFile(value.toString());
    }

    if (option == "debug") {
        setPrintDebugMessages(boolValue);
    }

    if (option == "disk-cache") {
        setDiskCacheEnabled(boolValue);
    }

    if (option == "ignore-ssl-errors") {
        setIgnoreSslErrors(boolValue);
    }

    if (option == "load-images") {
        setAutoLoadImages(boolValue);
    }

    if (option == "local-storage-path") {
        setOfflineStoragePath(value.toString());
    }

    if (option == "local-storage-quota") {
        setOfflineStorageDefaultQuota(value.toInt());
    }

    if (option == "local-to-remote-url-access") {
        setLocalToRemoteUrlAccessEnabled(boolValue);
    }

    if (option == "max-disk-cache") {
        setMaxDiskCacheSize(value.toInt());
    }

    if (option == "output-encoding") {
        setOutputEncoding(value.toString());
    }

    if (option == "remote-debugger-autorun") {
        setRemoteDebugAutorun(boolValue);
    }

    if (option == "remote-debugger-port") {
        setDebug(true);
        setRemoteDebugPort(value.toInt());
    }

    if (option == "proxy") {
        setProxy(value.toString());
    }

    if (option == "proxy-type") {
        setProxyType(value.toString());
    }

    if (option == "proxy-auth") {
        setProxyAuth(value.toString());
    }

    if (option == "script-encoding") {
        setScriptEncoding(value.toString());
    }

    if (option == "web-security") {
        setWebSecurityEnabled(boolValue);
    }

    if (option == "pac") {
        setProxyAutoConfig(value.toString());
    }
    
    if (option == "cert-authorities-path") {
        setCertAuthoritiesPath(value.toString());
    }
    
    if (option == "local-certificate-file") {
        setLocalCertificateFile(value.toString());
    }
    
    if (option == "local-certificate-passphrase") {
        setLocalCertificatePassPhrase(value.toString());
    }    
}

void Config::handleParam(const QString& param, const QVariant &value)
{
    Q_UNUSED(param);

    if (m_scriptFile.isEmpty())
        m_scriptFile = value.toString();
    else
        m_scriptArgs += value.toString();
}

void Config::handleError(const QString &error)
{
    setUnknownOption(QString("Error: %1").arg(error));
}

