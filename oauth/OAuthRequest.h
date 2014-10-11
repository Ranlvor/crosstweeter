#ifndef OAUTHREQUEST_H
#define OAUTHREQUEST_H
#include <QString>
#include <QByteArray>
#include <QtCrypto/QtCrypto>

class OAuthRequest
{
public:
    OAuthRequest(QString consumerKey, QString consumerSecret, QString tokenKey, QString tokenSecret);
    static QString hmacSha1(QByteArray key, QByteArray baseString);
    static QString generateNonce();
    void fillOauthParameters();
    void setGetParameters(QMap<QString, QString> paramters);
    void setPostParameters(QMap<QString, QString> paramters);
    void setRequestDestination(QString method, QString baseURL);
    void sign();
    QString getHeaderString();

private:
    QString nonce;
    QMap<QString, QString> getParamters;
    QMap<QString, QString> postParamters;
    QMap<QString, QString> oauthParameters;
    QString consumerKey;
    QString consumerSecret;
    QString tokenKey;
    QString tokenSecret;
    QString httpMethod;
    QString baseURL;
    QString signature;

};

#endif // OAUTHREQUEST_H
