#ifdef COROSSTWEETER
#include "crossTweeter.h"
#include <QCoreApplication>
#include <QTimer>
#include "dev-tokens.h"
#include "oauth/OAuthRequest.h"
#include "json/json.h"
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslConfiguration>

void resetNetworkRequest(QNetworkRequest * request){
    QSslConfiguration config = request->sslConfiguration();
    config.setCaCertificates(QSslCertificate::fromPath("../TwitterQT2/trusted rootCAs/*/*",QSsl::Pem,QRegExp::WildcardUnix));
    request->setSslConfiguration(config);
}

QString formatGetString(QString base, QMap<QString, QString> parameters){
    base.append("?");
    QMapIterator<QString, QString> i(parameters);
    while (i.hasNext()) {
        i.next();
        base.append(i.key());
        base.append("=");
        base.append(i.value());
        base.append("&");
    }
    base.chop(1);
    return base;
}

void checkMySQLError(QSqlQuery& q) {
    QSqlError e = q.lastError();
    if(!e.isValid())
        return;//no error
    qDebug()<<"got MySQL-Error"<<e.number()<<e.text()<<"/"<<e.databaseText()<<"/"<<e.driverText();
    qDebug()<<"on query"<<q.lastQuery();
    qDebug()<<"Terminating Application";
    QCoreApplication::quit();
}

crossTweeter::crossTweeter(QObject *parent) :
    QObject(parent),since_id(0), max_id(0), dirty(0), queryInsertMax(), queryInsertQueue()
{
}

void crossTweeter::main() {
    qDebug()<<"starting up, connecting mysql";

    pullManager = new QNetworkAccessManager(this);
    connect(pullManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(pullRequestFinished(QNetworkReply*)));
    pushManager = new QNetworkAccessManager(this);
    connect(pushManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(pushRequestFinished(QNetworkReply*)));
    streamManager = new QNetworkAccessManager(this);
    db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName(MYSQLdATABASEserver);
    db.setDatabaseName(MYSQLdATABASEname);
    db.setUserName(MYSQLdATABASEuser);
    db.setPassword(MYSQLdATABASEpassword);
    if (!db.open()){
        qFatal("Failed to connect to mysql");
        QCoreApplication::quit();
    }

    queryInsertMax = QSqlQuery(); //reset to new database
    queryInsertMax.prepare("INSERT INTO maxREST SET id = :new");

    queryInsertQueue = QSqlQuery(); //reset to new database
    queryInsertQueue.prepare("INSERT INTO queue SET id = :new");

    queryDelteToRetweet = QSqlQuery(); //reset to new database
    queryDelteToRetweet.prepare("DELETE FROM toretweet WHERE id = :new");

    queryInsertRetweeted = QSqlQuery(); //reset to new database
    queryInsertRetweeted.prepare("INSERT INTO retweeted SET id = :new");

    initSteam();
    initRESTPull();
}

void crossTweeter::initRESTPull(){
    dirty = true;

    QSqlQuery query2("BEGIN");
    checkMySQLError(query2);
    query2.finish();

    qDebug()<<"preparing REST-Request";
    QSqlQuery query("SELECT MAX( id ) FROM  `maxREST`");
    checkMySQLError(query);
    if (query.next()) {
        since_id = query.value(0).toLongLong();
    }

    doRESTPullRequest();
}

void crossTweeter::doRESTPullRequest(){
    QMap<QString, QString> parameters;
    parameters.insert("count", "200");
    if(since_id != 0)
        parameters.insert("since_id", QString::number(since_id));
    if(max_id != 0)
        parameters.insert("max_id", QString::number(max_id));
    OAuthRequest oauth(CONSUMERKEY, CONSUMERSECRET, TOKENKEY, TOKENSECRET);
    parameters.insert("screen_name", TARGET);
    parameters.insert("trim_user", "true");

    oauth.setGetParameters(parameters);

    oauth.setRequestDestination("GET", "https://api.twitter.com/1.1/statuses/user_timeline.json");

    oauth.sign();

    QNetworkRequest request = QNetworkRequest(QUrl(formatGetString("https://api.twitter.com/1.1/statuses/user_timeline.json", parameters)));

    request.setRawHeader(QString("Authorization").toAscii(), oauth.getHeaderString().toAscii());
    resetNetworkRequest(&request);

    qDebug()<<"sending REST-Request since"<<since_id<<"max"<<max_id;

    pullManager->get(request);
}

void crossTweeter::pullRequestFinished(QNetworkReply* reply){
    reply->deleteLater();
    qDebug()<<"got REST-Pull-Result";
    QByteArray data = reply->readAll();
    qDebug()<<"limit:"<<reply->rawHeader(QString("X-Rate-Limit-Remaining").toAscii())<<"/"<<reply->rawHeader(QString("X-Rate-Limit-Limit").toAscii());
    qDebug()<<"data: "<<data;

    bool ok;
    QList<QVariant> result = QtJson::Json::parse(data, ok).toList();

    if(!ok) {
        qFatal("An error occurred during parsing, rolling back and terminating");
        QSqlQuery query2("ROLLBACK");
        checkMySQLError(query2);
        query2.finish();
        QCoreApplication::quit();
    } else {
        if(result.length() < 1){
            qDebug()<<"commiting";
            QSqlQuery query2("COMMIT");
            checkMySQLError(query2);
            query2.finish();
            qDebug()<<"finished REST";
            doPendingRetweets();
        } else {
            foreach(QVariant tmp, result) {
                QVariantMap tweet = tmp.toMap();
                qint64 tweetID = tweet["id"].toLongLong();
                qDebug()<<"got ID"<<tweetID;
                queryInsertMax.bindValue("maxREST", tweetID);
                queryInsertMax.exec();
                checkMySQLError(queryInsertMax);
                queryInsertMax.finish();

                if(checkTweet(tweet)){
                    queryInsertQueue.bindValue("maxREST", tweetID);
                    queryInsertQueue.exec();
                    checkMySQLError(queryInsertQueue);
                    queryInsertQueue.finish();
                }

                max_id = tweetID -1;
            }
            doRESTPullRequest();
        }
    }
}

bool crossTweeter::checkTweet(QVariantMap tweet){
    if(tweet["text"].toString().startsWith("@")){
        qDebug()<<"rejected because first letter @";
        return false;
    }
    qDebug()<<"accepted";
    return true;
}

void crossTweeter::cleanup(){
    qDebug()<<"starting cleanup";
    QSqlQuery query1("BEGIN");
    checkMySQLError(query1);

    QSqlQuery query2("SELECT MAX( id ) FROM  `maxREST`");
    checkMySQLError(query2);
    if (query2.next()) {
        QSqlQuery query3;
        query3.prepare("DELETE FROM `maxREST` WHERE id < :max");
        query3.bindValue("max", query2.value(0).toLongLong());
        query3.exec();
        checkMySQLError(query3);
    }

    QSqlQuery query4("INSERT INTO toretweet "
                     "SELECT id FROM queue "
                     "WHERE id NOT IN (SELECT id FROM retweeted)");
    checkMySQLError(query4);

    QSqlQuery query5("DELETE FROM queue");
    checkMySQLError(query5);

    QSqlQuery query6("COMMIT");
    checkMySQLError(query6);

    qDebug()<<"cleanup done";
}

void crossTweeter::doPendingRetweets() {
    dirty = true;
    cleanup();
    doOldestPendingRetweet();

}

void crossTweeter::doOldestPendingRetweet() {
    QSqlQuery query("SELECT MIN(id) FROM toretweet LIMIT 1");
    if (query.next()) {
        retweeting = query.value(0).toLongLong();
        if(retweeting != 0) {
            OAuthRequest oauth(CONSUMERKEY, CONSUMERSECRET, TOKENKEY, TOKENSECRET);

            QMap<QString, QString> parameters;
            parameters.insert("trim_user", "true");

            oauth.setPostParameters(parameters);

            QString url = "https://api.twitter.com/1.1/statuses/retweet/";
            url.append(QVariant(retweeting).toString());
            url.append(".json");
            oauth.setRequestDestination("POST", url);

            oauth.sign();

            QNetworkRequest request = QNetworkRequest(QUrl(url));

            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

            request.setRawHeader(QString("Authorization").toAscii(), oauth.getHeaderString().toAscii());

            resetNetworkRequest(&request);

            QByteArray post;
            QMapIterator<QString, QString> i(parameters);
            while (i.hasNext()) {
                i.next();
                post.append(i.key());
                post.append("=");
                post.append(i.value());
                post.append("&");
            }

            qDebug()<<"sending Retweet-Request"<<retweeting;

            pushManager->post(request, post);
            return;
        }
    }

    dirty = false;
    pollStream();
}

void crossTweeter::pushRequestFinished(QNetworkReply* reply){
    reply->deleteLater();
    qDebug()<<"got retweet-Result (hopefully for"<<retweeting<<", error"<<reply->error()<<"("<<reply->errorString()<<"))";
    QByteArray data = reply->readAll();
    qDebug()<<"limit:"<<reply->rawHeader(QString("X-Rate-Limit-Remaining").toAscii())<<"/"<<reply->rawHeader(QString("X-Rate-Limit-Limit").toAscii());
    qDebug()<<"data: "<<data;

    bool ok;
    /*QList<QVariant> result = */QtJson::Json::parse(data, ok).toList();

    if(!ok || reply->error()) {
        qFatal("An error occurred during parsing or an error happened, terminating");
        QCoreApplication::quit();
    } else {
        qDebug()<<"sucess. Writing to Database";
        QSqlQuery q1("BEGIN");
        checkMySQLError(q1);

        queryInsertRetweeted.bindValue("new", retweeting);
        queryInsertRetweeted.exec();
        checkMySQLError(queryInsertRetweeted);
        queryInsertRetweeted.finish();

        queryDelteToRetweet.bindValue("new", retweeting);
        queryDelteToRetweet.exec();
        checkMySQLError(queryDelteToRetweet);
        queryDelteToRetweet.finish();

        QSqlQuery q2("COMMIT");
        checkMySQLError(q2);
        qDebug()<<"Database: done";

        doOldestPendingRetweet();
    }
}

void crossTweeter::initSteam(){
    qDebug()<<"connecting stream";
    OAuthRequest oauth(CONSUMERKEY, CONSUMERSECRET, TOKENKEY, TOKENSECRET);

    QMap<QString, QString> parameters;
    parameters.insert("follow", "2809152610");
    parameters.insert("delimited", "false");
    parameters.insert("stall_warnings", "true");

    oauth.setPostParameters(parameters);

    QString url = "https://stream.twitter.com/1.1/statuses/filter.json";
    //url = "https://userstream.twitter.com/1.1/user.json";
    //url = "http://thyristor.starletp9.de:11111/";
    oauth.setRequestDestination("POST", url);

    oauth.sign();

    QNetworkRequest request = QNetworkRequest(QUrl(url));

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    request.setRawHeader(QString("Authorization").toAscii(), oauth.getHeaderString().toAscii());
    request.setRawHeader(QString("Accept-Encoding").toAscii(),
                         QString("identity").toAscii());

    resetNetworkRequest(&request);

    QByteArray post;
    QMapIterator<QString, QString> i(parameters);
    while (i.hasNext()) {
        i.next();
        post.append(i.key());
        post.append("=");
        post.append(i.value());
        post.append("&");
    }
    post.chop(1);

    streamConnection = streamManager->post(request, post);

    connect(streamConnection, SIGNAL(readyRead()), this, SLOT(pollStream()));
    connect(streamConnection, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(streamError(QNetworkReply::NetworkError)));
}

void crossTweeter::pollStream(){
    if(dirty){
        qDebug()<<"pollStream(): poll aborted: dirty";
        return;
    }
    if(!streamConnection->canReadLine()){
        qDebug()<<"pollStream(): poll aborted: no line";
    }
    bool gotTweet = false;
    while (streamConnection->canReadLine()) {
        QByteArray line = streamConnection->readLine();
        if(line == "\n" || line == "\r" || line == "\r\n") {
            qDebug()<<"pollStream(): linebreak only ignored";
            break;
        }
        qDebug()<<"pollStream(): Got stream data:"<<line;


        bool ok;
        QVariantMap tweet = QtJson::Json::parse(line, ok).toMap();
        if(!ok) {
            qFatal("parsing error, exit");
            QCoreApplication::quit();
            return;
        }

        if(!tweet.contains("id")) {
            qDebug()<<"ignored: no tweetid";
            break;
        }

        if(!tweet.contains("text")) {
            qDebug()<<"ignored: no text";
            break;
        }

        if(checkTweet(tweet)){
            gotTweet = true;
            qint64 tweetID = tweet["id"].toLongLong();
            qDebug()<<"got ID"<<tweetID;
            queryInsertQueue.bindValue("maxREST", tweetID);
            queryInsertQueue.exec();
            checkMySQLError(queryInsertQueue);
            queryInsertQueue.finish();
        }
    }
    if(gotTweet)
        doPendingRetweets();
}

void crossTweeter::streamError(QNetworkReply::NetworkError /*e*/){
    qFatal((QString("Stream error ")+streamConnection->errorString()).toStdString().c_str());
    QCoreApplication::quit();
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    crossTweeter tlr;
    QTimer::singleShot(0, &tlr, SLOT(main()));

    return a.exec();
}
#endif //COROSSTWEETER
