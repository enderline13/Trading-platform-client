#ifndef TRADINGCLIENT_H
#define TRADINGCLIENT_H

#include <QObject>
#include <QGrpcCallReply>
#include <memory>
#include "trading_client.grpc.qpb.h"

class QAbstractGrpcChannel;

class TradingClient : public QObject
{
    Q_OBJECT
public:
    explicit TradingClient(std::shared_ptr<QAbstractGrpcChannel> channel,
                           const QString &token,
                           QObject *parent = nullptr);

    void placeOrder(const trading::PlaceOrderRequest &request);
    void getOrders(const trading::GetOrdersRequest &request);
    void cancelOrder(uint64_t orderId);

signals:
    void ordersReceived(const trading::Orders &orders);
    void ordersError(const QString &error);
    void orderCanceled(uint64_t orderId);
    void cancelError(const QString &error);
    void orderPlaced(const trading::PlaceOrderResponse &response);
    void orderError(const QString &error);

private:
    std::unique_ptr<trading::TradingService::Client> m_client;
    QString m_token;
};

#endif // TRADINGCLIENT_H
