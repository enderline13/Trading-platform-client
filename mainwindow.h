#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QChart>
#include <QCandlestickSeries>
#include <QDateTimeAxis>
#include <QValueAxis>

#include "auth.qpb.h"
#include "account.qpb.h"
#include "admin.qpb.h"
#include "marketclient.h"
#include "tradingclient.h"
#include "accountclient.h"
#include "adminclient.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onInstrumentsLoaded(const market::InstrumentsList &instruments);
    void onOrderBookUpdate(uint64_t instrumentId, const market::OrderBookUpdate &update);
    void onTradeReceived(uint64_t instrumentId, const common::Trade &trade);
    void onSendOrderClicked();
    void onOrderPlaced(const trading::PlaceOrderResponse &response);
    void onOrderError(const QString &error);
    void onOrderBookSnapshot(uint64_t instrumentId, const market::OrderBook &book);
    void onRefreshOrdersClicked();
    void onCancelOrderClicked();
    void onOrdersReceived(const trading::Orders &orders);
    void onOrdersError(const QString &error);
    void onOrderCanceled(uint64_t orderId);
    void onCancelError(const QString &error);
    void onRefreshHistoryClicked();
    void onTradeHistoryReceived(const trading::Trades &trades);
    void onTradeHistoryError(const QString &error);
    void onLoadCandlesClicked();
    void onCandlesReceived(const market::Candles &candles);
    void onCandlesError(const QString &error);
    void onCandleUpdate(uint64_t instrumentId, const common::Candle &candle);
    void onLiveCheckBoxToggled(bool checked);

    void onPortfolioTabActivated();
    void onDepositClicked();
    void onWithdrawClicked();
    void onBalanceReceived(const common::Decimal &balance);
    void onBalanceError(const QString &error);
    void onPositionsReceived(const account::UserPositions &positions);
    void onPositionsError(const QString &error);
    void onBalanceHistoryReceived(const account::BalanceHistory &history);
    void onBalanceHistoryError(const QString &error);
    void onDepositSuccess();
    void onDepositError(const QString &error);
    void onWithdrawSuccess();
    void onWithdrawError(const QString &error);

    void onAdminTabActivated();
    void onUpdateInstrumentClicked();
    void onAddBalanceClicked();
    void onAddPositionClicked();
    void onStartStopTradingClicked();
    void onInstrumentAdded();
    void onInstrumentUpdated();
    void onInstrumentError(const QString &error);
    void onFundSuccess();
    void onFundError(const QString &error);
    void onPositionAdded();
    void onPositionError(const QString &error);
    void onSystemStatusReceived(const admin::SystemStatus &status);
    void onSystemStatusError(const QString &error);
    void onSystemStateChanged(bool running);
    void onSystemStateError(const QString &error);
    void onAdminInstrumentSelected(int index);

private:
    void populateOrdersTable(const trading::Orders &orders);
    void populateTradesTable(const trading::Trades &trades);
    void setupChart();
    void clearChart();

    Ui::MainWindow *ui;

    QString m_token;
    auth::User::Role m_userRole = auth::User::Role::USER;

    MarketClient *m_marketClient = nullptr;
    TradingClient *m_tradingClient = nullptr;
    AccountClient *m_accountClient = nullptr;
    AdminClient *m_adminClient = nullptr;

    bool m_adminTradingRunning = false;

    uint64_t m_currentInstrumentId = 0;

    market::InstrumentsList m_cachedInstruments;

    uint64_t m_currentUserId = 0;

    QChart *m_chart = nullptr;
    QCandlestickSeries *m_candleSeries = nullptr;
    QDateTimeAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;

    uint64_t m_chartInstrumentId = 0;
    bool m_liveEnabled = false;

    QSet<uint64_t> m_userOrderIds;

    void showError(const QString &message);
};
#endif // MAINWINDOW_H
