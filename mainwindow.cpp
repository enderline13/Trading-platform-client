#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QGrpcHttp2Channel>
#include <QTimer>

#include "authclient.h"
#include "logindialog.h"

#include "proto_utils.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto channel = std::make_shared<QGrpcHttp2Channel>(QUrl("http://localhost:50051"));

    AuthClient auth(channel);
    LoginDialog loginDlg(&auth, this);
    if (loginDlg.exec() != QDialog::Accepted) {
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }

    m_token = loginDlg.token();
    m_userRole = loginDlg.role();

    if (m_userRole != auth::User::Role::ADMIN) {
        int idx = ui->tabWidget->indexOf(ui->admin_tab);
        if (idx != -1)
            ui->tabWidget->removeTab(idx);
    }

    // Инициализируем клиентов
    m_marketClient = new MarketClient(channel, m_token, this);
    m_tradingClient = new TradingClient(channel, m_token, this);

    // Сигналы MarketClient
    connect(m_marketClient, &MarketClient::instrumentsListed,
            this, &MainWindow::onInstrumentsLoaded);
    connect(m_marketClient, &MarketClient::listInstrumentsError,
            this, [](const QString &err) { qWarning() << "Market instruments error:" << err; });
    connect(m_marketClient, &MarketClient::orderBookUpdateReceived,
            this, &MainWindow::onOrderBookUpdate);
    connect(m_marketClient, &MarketClient::tradeReceived,
            this, &MainWindow::onTradeReceived);

    // Сигналы TradingClient
    connect(m_tradingClient, &TradingClient::orderPlaced,
            this, &MainWindow::onOrderPlaced);
    connect(m_tradingClient, &TradingClient::orderError,
            this, &MainWindow::onOrderError);

    connect(m_marketClient, &MarketClient::orderBookSnapshot,
            this, &MainWindow::onOrderBookSnapshot);

    // Загружаем список инструментов
    m_marketClient->listInstruments();

    // Настройка комбобоксов ордера
    QStringList orderTypes = {"LIMIT", "MARKET", "STOP"};
    ui->order_type->addItems(orderTypes);
    QStringList orderSides = {"BUY", "SELL"};
    ui->order_side->addItems(orderSides);

    // Блокировка цены для MARKET
    connect(ui->order_type, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        ui->order_price->setEnabled(text != "MARKET");
    });
    // Начальное состояние
    ui->order_price->setEnabled(ui->order_type->currentText() != "MARKET");

    // Кнопка отправки ордера
    connect(ui->send_order_button, &QPushButton::clicked, this, &MainWindow::onSendOrderClicked);

    // При смене инструмента переподписываемся
    connect(ui->instrument_select, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (index < 0) return;
                uint64_t instId = ui->instrument_select->currentData().toULongLong();
                if (instId == m_currentInstrumentId) return;

                m_currentInstrumentId = instId;

                // Очищаем старые данные
                ui->bids->setRowCount(0);
                ui->asks->setRowCount(0);
                ui->latest_trades->setRowCount(0);
                ui->best_bid_label->setText("—");
                ui->best_ask_label->setText("—");
                ui->sent_order_status->clear();

                // Подписываемся на стримы (обновления в реальном времени)
                m_marketClient->subscribeOrderBook(instId);
                m_marketClient->subscribeTrades(instId);

                // Запрашиваем мгновенный снапшот стакана
                m_marketClient->getOrderBook(instId);
            });

    // --- Orders tab ---
    // Подключаем сигналы
    connect(m_tradingClient, &TradingClient::ordersReceived,
            this, &MainWindow::onOrdersReceived);
    connect(m_tradingClient, &TradingClient::ordersError,
            this, &MainWindow::onOrdersError);
    connect(m_tradingClient, &TradingClient::orderCanceled,
            this, &MainWindow::onOrderCanceled);
    connect(m_tradingClient, &TradingClient::cancelError,
            this, &MainWindow::onCancelError);

    // Заполняем фильтры
    ui->order_by_status->addItem("All", QVariant::fromValue(common::Order::OrderStatus::NEW)); // или специальное значение
    // Заполняем статусы
    QStringList statuses = {"All", "NEW", "PARTIALLY_FILLED", "FILLED", "CANCELED", "REJECTED"};
    // Можно сохранить enum-значения через QVariant, для "All" -1
    // Заполним вручную с userData
    ui->order_by_status->clear();
    ui->order_by_status->addItem("All", -1);
    ui->order_by_status->addItem("NEW", static_cast<int>(common::Order::OrderStatus::NEW));
    ui->order_by_status->addItem("PARTIALLY_FILLED", static_cast<int>(common::Order::OrderStatus::PARTIALLY_FILLED));
    ui->order_by_status->addItem("FILLED", static_cast<int>(common::Order::OrderStatus::FILLED));
    ui->order_by_status->addItem("CANCELED", static_cast<int>(common::Order::OrderStatus::CANCELED));
    ui->order_by_status->addItem("REJECTED", static_cast<int>(common::Order::OrderStatus::REJECTED));

    // Заполняем фильтр по инструменту (после получения списка инструментов)
    // Для этого можно переиспользовать m_instrumentsList, сохранив его.
    // Предположим, мы сохраним список в член класса.
    // Добавим в mainwindow.h переменную market::InstrumentsList m_cachedInstruments;
    // Тогда после получения инструментов в onInstrumentsLoaded заполним комбобокс:
    // ...

    connect(ui->refresh_orders_button, &QPushButton::clicked,
            this, &MainWindow::onRefreshOrdersClicked);
    connect(ui->cancel_order_button, &QPushButton::clicked,
            this, &MainWindow::onCancelOrderClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onInstrumentsLoaded(const market::InstrumentsList &instruments)
{
    ui->instrument_select->blockSignals(true);
    ui->instrument_select->clear();
    for (const auto &inst : instruments.instruments()) {
        QString symbol = inst.symbol();
        uint64_t id = inst.id_proto();
        ui->instrument_select->addItem(symbol, QVariant::fromValue(id));
    }
    ui->instrument_select->blockSignals(false);

    if (ui->instrument_select->count() > 0) {
        ui->instrument_select->setCurrentIndex(0);
    }

    // Кэшируем
    m_cachedInstruments = instruments;

    // Заполняем фильтр инструментов вкладки Orders
    ui->order_by_instrument->clear();
    ui->order_by_instrument->addItem("All", 0);
    for (const auto &inst : instruments.instruments()) {
        ui->order_by_instrument->addItem(inst.symbol(), QVariant::fromValue(inst.id_proto()));
    }
}

void MainWindow::onOrderBookSnapshot(uint64_t instrumentId, const market::OrderBook &book)
{
    if (instrumentId != m_currentInstrumentId) return;
    // Используем уже существующую функцию, передав book как OrderBookUpdate?
    // OrderBook содержит repeated bids и asks, структура такая же, как в OrderBookUpdate.
    market::OrderBookUpdate update;
    update.setBids(book.bids());
    update.setAsks(book.asks());
    onOrderBookUpdate(instrumentId, update);
}

void MainWindow::onOrderBookUpdate(uint64_t instrumentId, const market::OrderBookUpdate &update)
{
    if (instrumentId != m_currentInstrumentId) return;

    const auto &bids = update.bids();
    ui->bids->setRowCount(bids.size());
    for (int i = 0; i < bids.size(); ++i) {
        const auto &level = bids.at(i);
        ui->bids->setItem(i, 0, new QTableWidgetItem(decimalToString(level.price())));
        ui->bids->setItem(i, 1, new QTableWidgetItem(decimalToString(level.quantity())));
    }

    const auto &asks = update.asks();
    ui->asks->setRowCount(asks.size());
    for (int i = 0; i < asks.size(); ++i) {
        const auto &level = asks.at(i);
        ui->asks->setItem(i, 0, new QTableWidgetItem(decimalToString(level.price())));
        ui->asks->setItem(i, 1, new QTableWidgetItem(decimalToString(level.quantity())));
    }

    // Лучшие цены
    if (!bids.isEmpty()) {
        ui->best_bid_label->setText(decimalToString(bids.first().price()));
    } else {
        ui->best_bid_label->setText("—");
    }

    if (!asks.isEmpty()) {
        ui->best_ask_label->setText(decimalToString(asks.first().price()));
    } else {
        ui->best_ask_label->setText("—");
    }
}

void MainWindow::onTradeReceived(uint64_t instrumentId, const common::Trade &trade)
{
    if (instrumentId != m_currentInstrumentId) return;

    int row = 0;
    ui->latest_trades->insertRow(row);

    // Время
    QDateTime dt = QDateTime::fromSecsSinceEpoch(trade.executedAt().seconds());
    ui->latest_trades->setItem(row, 0, new QTableWidgetItem(dt.toString("hh:mm:ss")));

    // Цена
    ui->latest_trades->setItem(row, 1, new QTableWidgetItem(decimalToString(trade.price())));

    // Количество
    ui->latest_trades->setItem(row, 2, new QTableWidgetItem(decimalToString(trade.quantity())));

    // Ограничение количества строк
    while (ui->latest_trades->rowCount() > 50)
        ui->latest_trades->removeRow(ui->latest_trades->rowCount() - 1);
}

void MainWindow::onSendOrderClicked()
{
    if (m_currentInstrumentId == 0) {
        ui->sent_order_status->setText("Выберите инструмент");
        return;
    }

    trading::PlaceOrderRequest req;
    req.setInstrumentId(m_currentInstrumentId);

    QString side = ui->order_side->currentText();
    QString type = ui->order_type->currentText();

    if (type == "LIMIT") {
        req.setType(common::Order::OrderType::LIMIT);
    } else if (type == "MARKET") {
        req.setType(common::Order::OrderType::MARKET);
        // Для MARKET цена игнорируется, но сервер может требовать 0
        req.mutPrice().setUnits(0);
        req.mutPrice().setNanos(0);
    } else if (type == "STOP") {
        req.setType(common::Order::OrderType::STOP);
    }

    if (side == "BUY") {
        req.setSide(common::Order::OrderSide::BUY);
    } else {
        req.setSide(common::Order::OrderSide::SELL);
    }

    // Количество
    QString qtyText = ui->order_quantity->text();
    // Простейший парсинг: ожидаем формат "x.y"
    QStringList parts = qtyText.split('.');
    int units = parts.value(0).toInt();
    int nanos = 0;
    if (parts.size() > 1) {
        QString frac = parts[1].leftJustified(9, '0'); // дополняем до 9 знаков
        nanos = frac.left(9).toInt();
    }
    req.mutQuantity().setUnits(units);
    req.mutQuantity().setNanos(nanos);

    // Цена
    if (type != "MARKET") {
        QString priceText = ui->order_price->text();
        QStringList pparts = priceText.split('.');
        int punits = pparts.value(0).toInt();
        int pnanos = 0;
        if (pparts.size() > 1) {
            QString pfrac = pparts[1].leftJustified(9, '0');
            pnanos = pfrac.left(9).toInt();
        }
        req.mutPrice().setUnits(punits);
        req.mutPrice().setNanos(pnanos);
    }

    m_tradingClient->placeOrder(req);
}

void MainWindow::onOrderPlaced(const trading::PlaceOrderResponse &response)
{
    ui->sent_order_status->setText("Order placed, ID=" + QString::number(response.orderId()));
}

void MainWindow::onOrderError(const QString &error)
{
    ui->sent_order_status->setText("Error: " + error);
}

void MainWindow::onRefreshOrdersClicked()
{
    trading::GetOrdersRequest req;

    // Статус
    int statusData = ui->order_by_status->currentData().toInt();
    if (statusData != -1) {
        auto status = static_cast<common::Order::OrderStatus>(statusData);
        req.setStatus(status);
    }

    // Инструмент
    uint64_t instId = ui->order_by_instrument->currentData().toULongLong();
    if (instId != 0) {
        req.setInstrumentId(instId);
    }

    m_tradingClient->getOrders(req);
}

void MainWindow::onCancelOrderClicked()
{
    if (ui->order_id_for_cancel->currentIndex() < 0) return;
    uint64_t orderId = ui->order_id_for_cancel->currentData().toULongLong();
    m_tradingClient->cancelOrder(orderId);
}

void MainWindow::onOrdersReceived(const trading::Orders &orders)
{
    populateOrdersTable(orders);

    // Заполняем комбобокс для отмены только активными ордерами (NEW, PARTIALLY_FILLED)
    ui->order_id_for_cancel->clear();
    for (const auto &order : orders.orders()) {
        if (order.status() == common::Order::OrderStatus::NEW || order.status() == common::Order::OrderStatus::PARTIALLY_FILLED) {
            ui->order_id_for_cancel->addItem(QString::number(order.id_proto()), QVariant::fromValue(order.id_proto()));
        }
    }
}

void MainWindow::onOrdersError(const QString &error)
{
    qWarning() << "Orders error:" << error;
}

void MainWindow::onOrderCanceled(uint64_t orderId)
{
    // Обновляем список после отмены
    onRefreshOrdersClicked();
}

void MainWindow::onCancelError(const QString &error)
{
    qWarning() << "Cancel error:" << error;
}

void MainWindow::populateOrdersTable(const trading::Orders &orders)
{
    ui->orders_table->setRowCount(orders.orders().size());
    for (int i = 0; i < orders.orders().size(); ++i) {
        const auto &o = orders.orders().at(i);
        auto set = [&](int col, const QString &text) {
            ui->orders_table->setItem(i, col, new QTableWidgetItem(text));
        };
        set(0, QString::number(o.id_proto()));
        // Для символа инструмента нужно найти по id из кэша
        QString symbol = QString::number(o.instrumentId());
        for (const auto &inst : m_cachedInstruments.instruments()) {
            if (inst.id_proto() == o.instrumentId()) {
                symbol = inst.symbol();
                break;
            }
        }
        set(1, symbol);
        set(2, [&]() -> QString {
            switch (o.type()) {
            case common::Order::OrderType::LIMIT: return "LIMIT";
            case common::Order::OrderType::MARKET: return "MARKET";
            case common::Order::OrderType::STOP: return "STOP";
            default: return "UNKNOWN";
            }
        }());
        set(3, o.side() == common::Order::OrderSide::BUY ? "BUY" : "SELL");
        set(4, decimalToString(o.price()));
        set(5, decimalToString(o.quantity()));
        set(6, decimalToString(o.remainingQuantity()));
        set(7, [&]() -> QString {
            switch (o.status()) {
            case common::Order::OrderStatus::NEW: return "NEW";
            case common::Order::OrderStatus::PARTIALLY_FILLED: return "PARTIAL";
            case common::Order::OrderStatus::FILLED: return "FILLED";
            case common::Order::OrderStatus::CANCELED: return "CANCELED";
            case common::Order::OrderStatus::REJECTED: return "REJECTED";
            default: return "?";
            }
        }());
        QDateTime dt = QDateTime::fromSecsSinceEpoch(o.createdAt().seconds());
        set(8, dt.toString("yyyy-MM-dd hh:mm:ss"));
    }
}
