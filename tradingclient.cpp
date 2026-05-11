#include "tradingclient.h"
#include "proto_utils.h"

TradingClient::TradingClient(std::shared_ptr<QAbstractGrpcChannel> channel,
                             const QString &token,
                             QObject *parent)
    : QObject(parent)
    , m_client(std::make_unique<trading::TradingService::Client>(nullptr))
    , m_token(token)
{
    m_client->attachChannel(channel);
}

void TradingClient::placeOrder(const trading::PlaceOrderRequest &request)
{
    auto reply = m_client->PlaceOrder(request, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<trading::PlaceOrderResponse>();
                    if (resp.has_value())
                        emit orderPlaced(resp.value());
                    else
                        emit orderError("Failed to read order response");
                } else {
                    emit orderError(status.message());
                }
            });
}

void TradingClient::getOrders(const trading::GetOrdersRequest &request)
{
    auto reply = m_client->GetOrders(request, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<trading::Orders>();
                    if (resp.has_value())
                        emit ordersReceived(resp.value());
                    else
                        emit ordersError("Failed to read orders");
                } else {
                    emit ordersError(status.message());
                }
            });
}

void TradingClient::cancelOrder(uint64_t orderId)
{
    trading::OrderID req;
    req.setOrderId(orderId);

    auto reply = m_client->CancelOrder(req, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, orderId, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    emit orderCanceled(orderId);
                } else {
                    emit cancelError(status.message());
                }
            });
}

void TradingClient::getTradeHistory(const trading::TradeHistoryRequest &request)
{
    auto reply = m_client->GetTradeHistory(request, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<trading::Trades>();
                    if (resp.has_value())
                        emit tradeHistoryReceived(resp.value());
                    else
                        emit tradeHistoryError("Failed to read trade history");
                } else {
                    emit tradeHistoryError(status.message());
                }
            });
}
