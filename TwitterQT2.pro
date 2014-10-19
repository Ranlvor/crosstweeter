#-------------------------------------------------
#
# Project created by QtCreator 2012-08-08T22:09:59
#
#-------------------------------------------------

QT       += core crypto qca network sql

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

LIBS += -lqca

DEFINES = COROSSTWEETER
