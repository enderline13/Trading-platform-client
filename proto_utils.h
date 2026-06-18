#ifndef GRPCHELPERS_H
#define GRPCHELPERS_H

#include <QGrpcCallOptions>
#include <QCandlestickSet>

#include "common.qpb.h"

inline QGrpcCallOptions makeCallOptions(const QString &token)
{
    QGrpcCallOptions options;
    options.setMetadata({{"authorization", token.toUtf8()}});
    return options;
}

inline QString decimalToString(const common::Decimal &d) {
    bool negative = (d.units() < 0 || d.nanos() < 0);
    qint64 absUnits = qAbs(static_cast<qint64>(d.units()));
    qint64 absNanos = qAbs(static_cast<qint64>(d.nanos()));
    QString result = QString("%1.%2")
                         .arg(absUnits)
                         .arg(absNanos, 9, 10, QChar('0'));
    if (negative)
        result.prepend('-');
    return result;
}

inline qreal decimalToDouble(const common::Decimal &d) {
    return d.units() + d.nanos() / 1e9;
}

inline QCandlestickSet *candleToSet(const common::Candle &c, QObject *parent = nullptr) {
    qreal open   = decimalToDouble(c.open());
    qreal high   = decimalToDouble(c.high());
    qreal low    = decimalToDouble(c.low());
    qreal close  = decimalToDouble(c.close());
    qint64 msecs = c.time().seconds() * 1000 + c.time().nanos() / 1e6;
    return new QCandlestickSet(open, high, low, close, msecs, parent);
}

#endif // GRPCHELPERS_H
