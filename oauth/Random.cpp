#include "Random.h"
#include <QMutex>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDebug>
#include <QThread>
#include <QtCrypto/QtCrypto>
static Random* m_Instance = 0;
Random::Random():sbox()
{
    sbox.fill(0,256);
    QByteArray key;
    key.append(QDateTime::currentDateTime().toString("d,dddd,M,MMMM,yyyy,h,m,s,z"));
    //todo: mehr random einfüllen
   // QThread::currentThreadId()
    qDebug()<<key;
    qDebug()<<key.length()<<"zufällige bytes";

    if(!QCA::isSupported("sha512"))
        exit(-1);
    QCA::Hash shaHash("sha512");
    shaHash.update(key);
    QCA::MemoryRegion hashedKey = shaHash.final();//reduziert den zufall aber auf 64 bytes

    initSBox(hashedKey.toByteArray());
}

Random* Random::instance()
{
    static QMutex mutex;
    if (!m_Instance)
    {
        mutex.lock();

        if (!m_Instance)
            m_Instance = new Random;

        mutex.unlock();
    }

    return m_Instance;
}
void Random::initSBox(QByteArray key){
    int l = key.length();
    for (int i = 0; i < 256; i++){
        sbox[i] = i;
    }
    qDebug()<<sbox.toBase64();
    /*

    j := 0
    Für i = 0 bis 255
      j := (j + s[i] + k[i mod L]) mod 256
      vertausche s[i] mit s[j]*/
    qDebug()<<sbox.toBase64();
}
