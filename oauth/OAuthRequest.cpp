#include "OAuthRequest.h"
#include <QCryptographicHash>
#include "Random.h"
#include <QUrl>
#include <QDebug>
OAuthRequest::OAuthRequest(QString consumerKey, QString consumerSecret, QString tokenKey, QString tokenSecret)
{
    this->consumerKey = consumerKey;
    this->consumerSecret = consumerSecret;
    this->tokenKey = tokenKey;
    this->tokenSecret = tokenSecret;
    nonce = generateNonce();
    fillOauthParameters();
}


QString OAuthRequest::generateNonce(){
    return QCA::Random::randomArray(32).toByteArray().toHex();
}

QString OAuthRequest::hmacSha1(QByteArray key, QByteArray baseString) {
    int blockSize = 64; // HMAC-SHA-1 block size, defined in SHA-1 standard
    if (key.length() > blockSize) { // if key is longer than block size (64), reduce key length with SHA-1 compression
        key = QCryptographicHash::hash(key, QCryptographicHash::Sha1);
    }

    QByteArray innerPadding(blockSize, char(0x36)); // initialize inner padding with char "6"
    QByteArray outerPadding(blockSize, char(0x5c)); // initialize outer padding with char "\"
    // ascii characters 0x36 ("6") and 0x5c ("\") are selected because they have large
    // Hamming distance (http://en.wikipedia.org/wiki/Hamming_distance)

    for (int i = 0; i < key.length(); i++) {
        innerPadding[i] = innerPadding[i] ^ key.at(i); // XOR operation between every byte in key and innerpadding, of key length
        outerPadding[i] = outerPadding[i] ^ key.at(i); // XOR operation between every byte in key and outerpadding, of key length
    }

    // result = hash ( outerPadding CONCAT hash ( innerPadding CONCAT baseString ) ).toBase64
    QByteArray total = outerPadding;
    QByteArray part = innerPadding;
    part.append(baseString);
    total.append(QCryptographicHash::hash(part, QCryptographicHash::Sha1));
    QByteArray hashed = QCryptographicHash::hash(total, QCryptographicHash::Sha1);
    return hashed.toBase64();
}

void OAuthRequest::setGetParameters(QMap<QString, QString> paramters){
    getParamters = paramters;
}
void OAuthRequest::setPostParameters(QMap<QString, QString> paramters){
    postParamters = paramters;
}
void OAuthRequest::setRequestDestination(QString method, QString url){
    httpMethod = method;
    baseURL = url;
}

void OAuthRequest::fillOauthParameters(){
    oauthParameters.clear();
    oauthParameters.insert("oauth_consumer_key", consumerKey);
    oauthParameters.insert("oauth_nonce", nonce);
    oauthParameters.insert("oauth_signature_method","HMAC-SHA1");
    oauthParameters.insert("oauth_timestamp", QVariant(QDateTime::currentDateTimeUtc().toTime_t()).toString());
    oauthParameters.insert("oauth_token", tokenKey);
    oauthParameters.insert("oauth_version", "1.0");
}
void OAuthRequest::sign(){
    QMap <QString, QString> parameters;
    {
        QMapIterator<QString, QString> i(oauthParameters);
        while (i.hasNext()) {
            i.next();
            parameters.insert(QUrl::toPercentEncoding(i.key()),
                              QUrl::toPercentEncoding(i.value()) );
        }
        QMapIterator<QString, QString> j(getParamters);
        while (j.hasNext()) {
            j.next();
            parameters.insert(QUrl::toPercentEncoding(j.key()),
                              QUrl::toPercentEncoding(j.value()) );
        }
        QMapIterator<QString, QString> k(postParamters);
        while (k.hasNext()) {
            k.next();
            parameters.insert(QUrl::toPercentEncoding(k.key()),
                              QUrl::toPercentEncoding(k.value()) );
        }
    }
    QString parameterString;
    QMapIterator<QString, QString> i(parameters);
    while (i.hasNext()) {
        i.next();
        parameterString.append(i.key());
        parameterString.append("=");
        parameterString.append(i.value());
        parameterString.append("&");
    }
    parameterString.chop(1);
    QString signatureBaseString;
    signatureBaseString.append(httpMethod.toUpper());
    signatureBaseString.append("&");
    signatureBaseString.append(QUrl::toPercentEncoding(baseURL));
    signatureBaseString.append("&");
    signatureBaseString.append(QUrl::toPercentEncoding(parameterString));
    QString signingKey;
    signingKey.append(QUrl::toPercentEncoding(consumerSecret));
    signingKey.append("&");
    signingKey.append(QUrl::toPercentEncoding(tokenSecret));
    signature = hmacSha1(signingKey.toAscii(), signatureBaseString.toAscii());
    oauthParameters.insert("oauth_signature", signature);
}

QString OAuthRequest::getHeaderString(){
    QString dst;
    dst.append("OAuth ");
    QMapIterator<QString, QString> i(oauthParameters);
    while (i.hasNext()) {
        i.next();
        dst.append(QUrl::toPercentEncoding(i.key()));
        dst.append("=\"");
        dst.append(QUrl::toPercentEncoding(i.value()));
        dst.append("\", ");
    }
    dst.chop(2);
    return dst;
}
