#ifdef COROSSTWEETER
#ifndef CROSSTWEETER_H
#define CROSSTWEETER_H

#include <QObject>
#include <QtSql>
#include <QSslSocket>
#include <QtCrypto/QtCrypto>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegExp>

class crossTweeter : public QObject
{
    Q_OBJECT
public:
    explicit crossTweeter(QObject *parent = 0);

public slots:
    void main();
    void initRESTPull();
    void doRESTPullRequest();
    void pullRequestFinished(QNetworkReply* reply);
    void pushRequestFinished(QNetworkReply* reply);
    void cleanup();
    void doPendingRetweets();
    void doOldestPendingRetweet();
    bool checkTweet(QVariantMap tweet);
    void initSteam();
    void pollStream();
    void streamError(QNetworkReply::NetworkError e);
    void streamTimeout();


private:
    QSqlDatabase db;
    qint64 since_id;
    qint64 max_id;
    QCA::Initializer init;
    QNetworkAccessManager * pullManager;
    QNetworkAccessManager * pushManager;
    QNetworkAccessManager * streamManager;
    QNetworkReply * streamConnection;

    bool dirty;
    QSqlQuery queryInsertMax;
    QSqlQuery queryInsertQueue;

    QSqlQuery queryDelteToRetweet;
    QSqlQuery queryInsertRetweeted;
    qint64 retweeting;

    QTimer streamPollTimeoutTimer;

    static const int streamReadTimeoutMS = 1000 * 60 * 10; //10 Minutes
};

#endif // CROSSTWEETER_H
#endif //COROSSTWEETER
