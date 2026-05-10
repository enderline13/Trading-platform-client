#ifndef GRPCHELPERS_H
#define GRPCHELPERS_H

#include <QGrpcCallOptions>

inline QGrpcCallOptions makeCallOptions(const QString &token)
{
    QGrpcCallOptions options;
    options.setMetadata({{"authorization", token.toUtf8()}});
    return options;
}

#endif // GRPCHELPERS_H
