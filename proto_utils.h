#ifndef GRPCHELPERS_H
#define GRPCHELPERS_H

#include <QGrpcCallOptions>

#include "common.qpb.h"

inline QGrpcCallOptions makeCallOptions(const QString &token)
{
    QGrpcCallOptions options;
    options.setMetadata({{"authorization", token.toUtf8()}});
    return options;
}

inline QString decimalToString(const common::Decimal &d) {
    return QString("%1.%2").arg((qlonglong)d.units()).arg((qlonglong)d.nanos(), 9, 10, QChar('0'));
}

#endif // GRPCHELPERS_H
