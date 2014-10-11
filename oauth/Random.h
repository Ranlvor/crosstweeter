#ifndef RANDOM_H
#define RANDOM_H
#include <QByteArray>
#include <QMutex>

class Random
{
public:
    static Random* instance();

private:
    Random();
    Random(const Random &);
    Random& operator=(const Random &);

    QByteArray sbox;
    void initSBox(QByteArray key);
    //static Random* m_Instance;

};

#endif // RANDOM_H
