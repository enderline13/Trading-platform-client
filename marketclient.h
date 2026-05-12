#ifndef MARKETCLIENT_H
#define MARKETCLIENT_H

#include <QObject>
#include <QGrpcCallReply>
#include <memory>
#include <map>
#include <cstdint>

#include "market_client.grpc.qpb.h"
#include "common.qpb.h"

class QAbstractGrpcChannel;
class QGrpcServerStream;

class MarketClient : public QObject
{
    Q_OBJECT
public:
    explicit MarketClient(std::shared_ptr<QAbstractGrpcChannel> channel,
                          const QString &token,
                          QObject *parent = nullptr);

    void listInstruments();
    void subscribeOrderBook(uint64_t instrumentId);
    void subscribeTrades(uint64_t instrumentId);
    void getOrderBook(uint64_t instrumentId);
    void getCandles(const market::CandlesRequest &request);
    void subscribeCandles(uint64_t instrumentId);
    void unsubscribeCandles(uint64_t instrumentId);

signals:
    void candlesReceived(const market::Candles &candles);
    void candlesError(const QString &error);
    void candleUpdateReceived(uint64_t instrumentId, const common::Candle &candle);
    void orderBookSnapshot(uint64_t instrumentId, const market::OrderBook &book);
    void instrumentsListed(const market::InstrumentsList &instruments);
    void listInstrumentsError(const QString &error);

    void orderBookUpdateReceived(uint64_t instrumentId, const market::OrderBookUpdate &update);
    void tradeReceived(uint64_t instrumentId, const common::Trade &trade);

private:
    std::unique_ptr<market::MarketService::Client> m_client;
    QString m_token;
    std::map<uint64_t, std::shared_ptr<QGrpcServerStream>> m_orderBookStreams;
    std::map<uint64_t, std::shared_ptr<QGrpcServerStream>> m_tradeStreams;
    std::map<uint64_t, std::shared_ptr<QGrpcServerStream>> m_candleStreams;
};

#endif // MARKETCLIENT_H
