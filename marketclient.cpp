#include "marketclient.h"
#include "proto_utils.h"
#include <google/protobuf/empty.qpb.h>
#include <QGrpcServerStream>
#include <QDebug>

MarketClient::MarketClient(std::shared_ptr<QAbstractGrpcChannel> channel,
                           const QString &token,
                           QObject *parent)
    : QObject(parent)
    , m_client(std::make_unique<market::MarketService::Client>(nullptr))
    , m_token(token)
{
    m_client->attachChannel(channel);
}

void MarketClient::listInstruments()
{
    google::protobuf::Empty empty;
    auto reply = m_client->ListInstruments(empty, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<market::InstrumentsList>();
                    if (resp.has_value())
                        emit instrumentsListed(resp.value());
                    else
                        emit listInstrumentsError("Failed to read instruments list");
                } else {
                    emit listInstrumentsError(status.message());
                }
            });
}

void MarketClient::subscribeOrderBook(uint64_t instrumentId)
{
    if (m_orderBookStreams.count(instrumentId)) return;

    market::InstrumentID req;
    req.setInstrumentId(instrumentId);

    auto stream = m_client->StreamOrderBook(req, makeCallOptions(m_token));
    auto streamPtr = std::shared_ptr<QGrpcServerStream>(stream.release());

    connect(streamPtr.get(), &QGrpcServerStream::messageReceived, this,
            [this, instrumentId, streamPtr]() {
                auto msg = streamPtr->read<market::OrderBookUpdate>();
                if (msg.has_value())
                    emit orderBookUpdateReceived(instrumentId, msg.value());
            });

    connect(streamPtr.get(), &QGrpcServerStream::finished, this,
            [this, instrumentId](const QGrpcStatus &) {
                m_orderBookStreams.erase(instrumentId);
            });

    m_orderBookStreams[instrumentId] = streamPtr;
}

void MarketClient::subscribeTrades(uint64_t instrumentId)
{
    if (m_tradeStreams.count(instrumentId)) return;

    market::InstrumentID req;
    req.setInstrumentId(instrumentId);

    auto stream = m_client->StreamTrades(req, makeCallOptions(m_token));
    auto streamPtr = std::shared_ptr<QGrpcServerStream>(stream.release());

    connect(streamPtr.get(), &QGrpcServerStream::messageReceived, this,
            [this, instrumentId, streamPtr]() {
                auto msg = streamPtr->read<common::Trade>();
                if (msg.has_value())
                    emit tradeReceived(instrumentId, msg.value());
            });

    connect(streamPtr.get(), &QGrpcServerStream::finished, this,
            [this, instrumentId](const QGrpcStatus &) {
                m_tradeStreams.erase(instrumentId);
            });

    m_tradeStreams[instrumentId] = streamPtr;
}

void MarketClient::getOrderBook(uint64_t instrumentId)
{
    market::InstrumentID req;
    req.setInstrumentId(instrumentId);

    auto reply = m_client->GetOrderBook(req, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, instrumentId, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<market::OrderBook>();
                    if (resp.has_value())
                        emit orderBookSnapshot(instrumentId, resp.value());
                }
            });
}
