#-------------------------------------------------
#
# Project created by QtCreator 2012-08-08T22:09:59
#
#-------------------------------------------------

QT       += core gui crypto qca network xml sql

TARGET = TwitterQT2
TEMPLATE = app


SOURCES +=\
    oauth/OAuthRequest.cpp \
    json/json.cpp \
    tools/crossTweeter.cpp

HEADERS  += \
    oauth/OAuthRequest.h \
    dev-tokens.h \
    json/json.h \
    tools/crossTweeter.h

FORMS    +=

LIBS += -lqca

DEFINES = COROSSTWEETER
