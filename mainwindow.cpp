#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QGrpcHttp2Channel>
#include <QTimer>

#include "authclient.h"
#include "logindialog.h"

#include <QGrpcChannelOptions>
#include <QMessageBox>

#include "proto_utils.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->bids->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->asks->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->latest_trades->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->orders_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->trades_history_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->user_positions_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->balance_history_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->users_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->user_role_combo->addItems({"USER", "ADMIN"});


    auto channel = std::make_shared<QGrpcHttp2Channel>(QUrl("http://localhost:50051"));
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    QGrpcChannelOptions opt;
    opt.setSslConfiguration(sslConfig);
    channel->setChannelOptions(opt);

    AuthClient auth(channel);
    LoginDialog loginDlg(&auth, this);
    if (loginDlg.exec() != QDialog::Accepted) {
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }

    m_token = loginDlg.token();
    m_userRole = loginDlg.role();
    m_currentUserId = loginDlg.userId();
    // Загружаем все ордера пользователя, чтобы кэшировать их ID

    if (m_userRole != auth::User::Role::ADMIN) {
        int idx = ui->tabWidget->indexOf(ui->admin_tab);
        if (idx != -1)
            ui->tabWidget->removeTab(idx);
    }

    // Инициализируем клиентов
    m_marketClient = new MarketClient(channel, m_token, this);
    m_tradingClient = new TradingClient(channel, m_token, this);
    m_accountClient = new AccountClient(channel, m_token, this);
    m_adminClient = new AdminClient(channel, m_token, this);

    trading::GetOrdersRequest allOrdersReq;
    // без фильтров – получим ордера всех статусов и по всем инструментам
    m_tradingClient->getOrders(allOrdersReq);
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
            this, &MainWindow::onInstrumentChanged);

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
    ui->order_by_status->addItem("Все", QVariant::fromValue(common::Order::OrderStatus::NEW)); // или специальное значение
    // Заполняем статусы
    QStringList statuses = {"Все", "NEW", "PARTIALLY_FILLED", "FILLED", "CANCELED", "REJECTED"};
    // Можно сохранить enum-значения через QVariant, для "All" -1
    // Заполним вручную с userData
    ui->order_by_status->clear();
    ui->order_by_status->addItem("Все", -1);
    ui->order_by_status->addItem("NEW", static_cast<int>(common::Order::OrderStatus::NEW));
    ui->order_by_status->addItem("PARTIALLY_FILLED", static_cast<int>(common::Order::OrderStatus::PARTIALLY_FILLED));
    ui->order_by_status->addItem("FILLED", static_cast<int>(common::Order::OrderStatus::FILLED));
    ui->order_by_status->addItem("CANCELED", static_cast<int>(common::Order::OrderStatus::CANCELED));
    ui->order_by_status->addItem("REJECTED", static_cast<int>(common::Order::OrderStatus::REJECTED));


    connect(ui->refresh_orders_button, &QPushButton::clicked,
            this, &MainWindow::onRefreshOrdersClicked);
    connect(ui->cancel_order_button, &QPushButton::clicked,
            this, &MainWindow::onCancelOrderClicked);


    // History tab
    connect(m_tradingClient, &TradingClient::tradeHistoryReceived,
            this, &MainWindow::onTradeHistoryReceived);
    connect(m_tradingClient, &TradingClient::tradeHistoryError,
            this, &MainWindow::onTradeHistoryError);
    connect(ui->refresh_history_button, &QPushButton::clicked,
            this, &MainWindow::onRefreshHistoryClicked);

    // --- Chart tab ---
    connect(m_marketClient, &MarketClient::candlesReceived,
            this, &MainWindow::onCandlesReceived);
    connect(m_marketClient, &MarketClient::candlesError,
            this, &MainWindow::onCandlesError);
    connect(m_marketClient, &MarketClient::candleUpdateReceived,
            this, &MainWindow::onCandleUpdate);
    connect(ui->load_history_candes, &QPushButton::clicked,
            this, &MainWindow::onLoadCandlesClicked);
    connect(ui->live_chart_update, &QCheckBox::toggled,
            this, &MainWindow::onLiveCheckBoxToggled);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this,
            [this](int index) {
                if (ui->tabWidget->widget(index) == ui->history_tab) {
                    // При переходе на вкладку истории сразу запрашиваем ордера
                    trading::GetOrdersRequest req;
                    m_tradingClient->getOrders(req);
                }
            });


    // --- Portfolio tab ---
    connect(m_accountClient, &AccountClient::balanceReceived,
            this, &MainWindow::onBalanceReceived);
    connect(m_accountClient, &AccountClient::balanceError,
            this, &MainWindow::onBalanceError);   // вместо лямбды

    connect(m_accountClient, &AccountClient::positionsReceived,
            this, &MainWindow::onPositionsReceived);
    connect(m_accountClient, &AccountClient::positionsError,
            this, &MainWindow::onPositionsError); // вместо лямбды

    connect(m_accountClient, &AccountClient::balanceHistoryReceived,
            this, &MainWindow::onBalanceHistoryReceived);
    connect(m_accountClient, &AccountClient::balanceHistoryError,
            this, &MainWindow::onBalanceHistoryError); // вместо лямбды

    connect(m_accountClient, &AccountClient::depositSuccess,
            this, &MainWindow::onDepositSuccess);
    connect(m_accountClient, &AccountClient::depositError,
            this, &MainWindow::onDepositError);
    connect(m_accountClient, &AccountClient::withdrawSuccess,
            this, &MainWindow::onWithdrawSuccess);
    connect(m_accountClient, &AccountClient::withdrawError,
            this, &MainWindow::onWithdrawError);

    connect(ui->deposit_button, &QPushButton::clicked,
            this, &MainWindow::onDepositClicked);
    connect(ui->withdraw_button, &QPushButton::clicked,
            this, &MainWindow::onWithdrawClicked);

    // Автообновление при открытии вкладки Portfolio
    connect(ui->tabWidget, &QTabWidget::currentChanged, this,
            [this](int index) {
                if (ui->tabWidget->widget(index) == ui->portfolio_tab) {
                    m_accountClient->getBalance();
                    m_accountClient->getPositions();
                    m_accountClient->getBalanceHistory();
                }
                if (ui->tabWidget->widget(index) == ui->admin_tab) {
                    onAdminTabActivated();     // уже есть (загружает системный статус)
                    m_adminClient->listUsers(); // теперь ещё и пользователей
                }
            });

    // --- Admin tab ---
    connect(m_adminClient, &AdminClient::instrumentAdded,
            this, &MainWindow::onInstrumentAdded);
    connect(m_adminClient, &AdminClient::instrumentUpdated,
            this, &MainWindow::onInstrumentUpdated);
    connect(m_adminClient, &AdminClient::instrumentError,
            this, &MainWindow::onInstrumentError);
    connect(m_adminClient, &AdminClient::fundSuccess,
            this, &MainWindow::onFundSuccess);
    connect(m_adminClient, &AdminClient::fundError,
            this, &MainWindow::onFundError);
    connect(m_adminClient, &AdminClient::positionAdded,
            this, &MainWindow::onPositionAdded);
    connect(m_adminClient, &AdminClient::positionError,
            this, &MainWindow::onPositionError);
    connect(m_adminClient, &AdminClient::systemStatusReceived,
            this, &MainWindow::onSystemStatusReceived);
    connect(m_adminClient, &AdminClient::systemStatusError,
            this, &MainWindow::onSystemStatusError);
    connect(m_adminClient, &AdminClient::systemStateChanged,
            this, &MainWindow::onSystemStateChanged);
    connect(m_adminClient, &AdminClient::systemStateError,
            this, &MainWindow::onSystemStateError);

    connect(ui->update_instrument_admin, &QPushButton::clicked,
            this, &MainWindow::onUpdateInstrumentClicked);
    connect(ui->add_balance_button, &QPushButton::clicked,
            this, &MainWindow::onAddBalanceClicked);
    connect(ui->add_position_button, &QPushButton::clicked,
            this, &MainWindow::onAddPositionClicked);
    connect(ui->start_stop_trading_button, &QPushButton::clicked,
            this, &MainWindow::onStartStopTradingClicked);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this,
            [this](int index) {
                if (ui->tabWidget->widget(index) == ui->admin_tab) {
                    onAdminTabActivated();
                }
            });

    connect(ui->instrument_select_admin, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onAdminInstrumentSelected);

    // Удаление инструмента
    connect(ui->delete_instrument_button, &QPushButton::clicked, this, &MainWindow::onDeleteInstrumentClicked);
    connect(m_adminClient, &AdminClient::instrumentDeleted, this, [this]() {
        QMessageBox::information(this, "Успех", "Инструмент удалён");
        m_marketClient->listInstruments(); // обновить все списки
    });
    connect(m_adminClient, &AdminClient::deleteInstrumentError, this, [this](const QString &err) {
        QMessageBox::warning(this, "Ошибка", err);
    });

    // Пользователи
    connect(ui->refresh_users_button, &QPushButton::clicked, this, [this]() {
        m_adminClient->listUsers();
    });
    connect(m_adminClient, &AdminClient::usersListed, this, &MainWindow::onUsersListed);
    connect(m_adminClient, &AdminClient::listUsersError, this, [this](const QString &err) {
        QMessageBox::warning(this, "Ошибка", err);
    });

    connect(ui->apply_user_button, &QPushButton::clicked, this, &MainWindow::onApplyUserChanges);
    connect(m_adminClient, &AdminClient::userRoleSet, this, [this]() {
        m_adminClient->listUsers(); // обновить таблицу
    });
    connect(m_adminClient, &AdminClient::userRoleSetError, this, [this](const QString &err) {
        QMessageBox::warning(this, "Ошибка", err);
    });
    connect(m_adminClient, &AdminClient::userActiveSet, this, [this]() {
        m_adminClient->listUsers();
    });
    connect(m_adminClient, &AdminClient::userActiveSetError, this, [this](const QString &err) {
        QMessageBox::warning(this, "Ошибка", err);
    });

    connect(m_adminClient, &AdminClient::fundSuccess,
            this, &MainWindow::onFundSuccess);
    connect(m_adminClient, &AdminClient::fundError,
            this, &MainWindow::onFundError);
    connect(m_adminClient, &AdminClient::positionAdded,
            this, &MainWindow::onPositionAdded);
    connect(m_adminClient, &AdminClient::positionError,
            this, &MainWindow::onPositionError);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::critical(this, "Предупреждение", message);
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
        // Принудительно инициируем загрузку первого инструмента
        onInstrumentChanged(0);
    }

    // Кэшируем
    m_cachedInstruments = instruments;

    // Заполняем фильтр инструментов вкладки Orders
    ui->order_by_instrument->clear();
    ui->order_by_instrument->addItem("Все", 0);
    for (const auto &inst : instruments.instruments()) {
        ui->order_by_instrument->addItem(inst.symbol(), QVariant::fromValue(inst.id_proto()));
    }

    ui->instrument_select_for_history_button->clear();
    ui->instrument_select_for_history_button->addItem("Все", 0);
    for (const auto &inst : instruments.instruments()) {
        ui->instrument_select_for_history_button->addItem(inst.symbol(), QVariant::fromValue(inst.id_proto()));
    }

    ui->instrument_select_for_chart->clear();
    for (const auto &inst : instruments.instruments()) {
        ui->instrument_select_for_chart->addItem(inst.symbol(), QVariant::fromValue(inst.id_proto()));
    }

    setupChart();

    ui->instrument_select_admin->clear();
    ui->instrument_select_admin->addItem("Новый инструмент", QVariant::fromValue(uint64_t(0)));
    for (const auto &inst : instruments.instruments()) {
        ui->instrument_select_admin->addItem(inst.symbol(), QVariant::fromValue(inst.id_proto()));
    }

    ui->instrument_id_positions->clear();
    for (const auto &inst : instruments.instruments()) {
        ui->instrument_id_positions->addItem(inst.symbol(), QVariant::fromValue(inst.id_proto()));
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

    // Для P&L: вычисляем середину спреда
    if (!book.bids().isEmpty() && !book.asks().isEmpty()) {
        double bestBid = decimalToDouble(book.bids().first().price());
        double bestAsk = decimalToDouble(book.asks().first().price());
        double mid = (bestBid + bestAsk) / 2.0;
        m_lastPrices[instrumentId] = mid; // сохраняем mid как последнюю цену
    }

    // Если вкладка портфеля активна, перерисуем позиции
    if (ui->tabWidget->currentWidget() == ui->portfolio_tab) {
        refreshPositionsTable();
    }
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

    QDateTime dt = QDateTime::fromSecsSinceEpoch(trade.executedAt().seconds());
    ui->latest_trades->setItem(row, 0, new QTableWidgetItem(dt.toString("hh:mm:ss")));
    ui->latest_trades->setItem(row, 1, new QTableWidgetItem(decimalToString(trade.price())));
    ui->latest_trades->setItem(row, 2, new QTableWidgetItem(decimalToString(trade.quantity())));

    while (ui->latest_trades->rowCount() > 50)
        ui->latest_trades->removeRow(ui->latest_trades->rowCount() - 1);

    // Сохраняем последнюю цену для P&L
   m_lastPrices[instrumentId] = decimalToDouble(trade.price());

    // Показываем уведомление в статусбаре
    QString symbol;
    for (const auto &inst : m_cachedInstruments.instruments()) {
        if (inst.id_proto() == instrumentId) {
            symbol = inst.symbol();
            break;
        }
    }
    if (symbol.isEmpty()) symbol = QString::number(instrumentId);
    ui->statusbar->showMessage(
        QString("Trade: %1 @ %2 x %3")
            .arg(symbol, decimalToString(trade.price()), decimalToString(trade.quantity())),
        3000);
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
    m_userOrderIds.clear();
    for (const auto &order : orders.orders()) {
        m_userOrderIds.insert(order.id_proto());
    }

    populateOrdersTable(orders);

    // Заполняем комбобокс для отмены только активными ордерами (NEW, PARTIALLY_FILLED)
    ui->order_id_for_cancel->clear();
    for (const auto &order : orders.orders()) {
        if (order.status() == common::Order::OrderStatus::NEW || order.status() == common::Order::OrderStatus::PARTIALLY_FILLED) {
            ui->order_id_for_cancel->addItem(QString::number(order.id_proto()), QVariant::fromValue(order.id_proto()));
        }
    }

    if (ui->tabWidget->currentWidget() == ui->history_tab) {
        onRefreshHistoryClicked();
    }
}

void MainWindow::onOrdersError(const QString &error)
{
    showError("Ошибка ордера:" + error);
}

void MainWindow::onOrderCanceled(uint64_t orderId)
{
    // Обновляем список после отмены
    onRefreshOrdersClicked();
}

void MainWindow::onCancelError(const QString &error)
{
    showError("Ошибка во время отмены:" + error);
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

void MainWindow::onRefreshHistoryClicked()
{
    trading::TradeHistoryRequest req;
    uint64_t instId = ui->instrument_select_for_history_button->currentData().toULongLong();
    if (instId != 0) {
        req.setInstrumentId(instId);
    }
    m_tradingClient->getTradeHistory(req);
}

void MainWindow::onTradeHistoryReceived(const trading::Trades &trades)
{
    populateTradesTable(trades);
}

void MainWindow::onTradeHistoryError(const QString &error)
{
    showError("Ошибка истории сделок:" + error);
}

void MainWindow::populateTradesTable(const trading::Trades &trades)
{
    const auto &list = trades.trades(); // или .trades_list() в зависимости от генерации
    ui->trades_history_table->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i) {
        const auto &t = list[i];
        auto set = [&](int col, const QString &text) {
            ui->trades_history_table->setItem(i, col, new QTableWidgetItem(text));
        };
        set(0, QString::number(t.id_proto()));

        // Символ инструмента
        QString symbol = QString::number(t.instrumentId());
        for (const auto &inst : m_cachedInstruments.instruments()) {
            if (inst.id_proto() == t.instrumentId()) {
                symbol = inst.symbol();
                break;
            }
        }
        set(1, symbol);

        set(2, decimalToString(t.price()));
        set(3, decimalToString(t.quantity()));

        QDateTime dt = QDateTime::fromSecsSinceEpoch(t.executedAt().seconds());
        set(4, dt.toString("yyyy-MM-dd hh:mm:ss"));

        // Определяем роль
        QString role = "—";
        if (!m_userOrderIds.isEmpty()) {
            // Если buy_order_id принадлежит нам, значит мы покупатель
            if (m_userOrderIds.contains(t.buyOrderId()))
                role = "Покупатель";
            else if (m_userOrderIds.contains(t.sellOrderId()))
                role = "Продавец";
            // (второе условие можно просто else, т.к. один из ордеров наш)
        }
        set(5, role);
    }
}

void MainWindow::setupChart()
{
    m_chart = new QChart();
    m_chart->setTitle("График свечей");
    m_chart->setAnimationOptions(QChart::SeriesAnimations);

    m_candleSeries = new QCandlestickSeries();
    m_candleSeries->setName("Цена");
    m_candleSeries->setIncreasingColor(QColor(Qt::green));
    m_candleSeries->setDecreasingColor(QColor(Qt::red));
    m_chart->addSeries(m_candleSeries);

    m_axisX = new QDateTimeAxis();
    m_axisX->setFormat("dd.MM hh:mm");
    m_axisX->setTitleText("Время");
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_candleSeries->attachAxis(m_axisX);

    m_axisY = new QValueAxis();
    m_axisY->setTitleText("Цена");
    m_axisY->setLabelFormat("%.2f");
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_candleSeries->attachAxis(m_axisY);

    m_axisX->setRange(QDateTime::currentDateTime().addSecs(-3600),
                      QDateTime::currentDateTime());
    m_axisY->setRange(0.0, 100000.0);

    ui->chart_view->setChart(m_chart);
    ui->chart_view->setRenderHint(QPainter::Antialiasing);
}

void MainWindow::clearChart()
{
    m_candleSeries->clear();
    // Не удаляем оси, просто очищаем свечи
}

void MainWindow::onLoadCandlesClicked()
{
    uint64_t instId = ui->instrument_select_for_chart->currentData().toULongLong();
    if (instId == 0) return;

    // Останавливаем live-обновления и отписываемся от стрима
    if (m_liveEnabled) {
        ui->live_chart_update->setChecked(false);  // вызовет onLiveCheckBoxToggled(false)
        m_liveEnabled = false;
    }
    m_marketClient->unsubscribeCandles(m_chartInstrumentId);

    m_chartInstrumentId = instId;

    market::CandlesRequest req;
    req.setInstrumentId(instId);

    QDateTime start = ui->start_candle_date->dateTime();
    QDateTime end   = ui->last_candle_date->dateTime();

    google::protobuf::Timestamp& ts_begin = req.mutBegin();
    ts_begin.setSeconds(start.toSecsSinceEpoch());
    ts_begin.setNanos(0);

    google::protobuf::Timestamp& ts_end = req.mutEnd();
    ts_end.setSeconds(end.toSecsSinceEpoch());
    ts_end.setNanos(0);

    // limit не задаём, пусть сервер пришлёт всё за период
    m_marketClient->getCandles(req);
}

void MainWindow::onCandlesReceived(const market::Candles &candles)
{
    clearChart();   // удаляем только свечи, оси остаются

    const auto &list = candles.candles();
    if (list.isEmpty()) return;

    for (const auto &c : list) {
        QCandlestickSet *set = candleToSet(c, m_candleSeries);
        m_candleSeries->append(set);
    }

    // Пересчитываем диапазоны
    qreal minPrice = std::numeric_limits<qreal>::max();
    qreal maxPrice = std::numeric_limits<qreal>::lowest();
    qint64 minTime = std::numeric_limits<qint64>::max();
    qint64 maxTime = std::numeric_limits<qint64>::lowest();
    for (const auto &c : list) {
        qint64 t = c.time().seconds() * 1000 + c.time().nanos() / 1e6;
        qreal high = decimalToDouble(c.high());
        qreal low  = decimalToDouble(c.low());
        minTime = std::min(minTime, t);
        maxTime = std::max(maxTime, t);
        minPrice = std::min(minPrice, low);
        maxPrice = std::max(maxPrice, high);
    }
    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(minTime),
                      QDateTime::fromMSecsSinceEpoch(maxTime));
    m_axisY->setRange(minPrice * 0.999, maxPrice * 1.001);
}

void MainWindow::onCandlesError(const QString &error)
{
    showError("Ошибка свеч:" + error);
}

void MainWindow::onCandleUpdate(uint64_t instrumentId, const common::Candle &candle)
{
    if (!m_liveEnabled || instrumentId != m_chartInstrumentId) return;

    qint64 newTime = candle.time().seconds() * 1000 + candle.time().nanos() / 1e6;
    QList<QCandlestickSet *> sets = m_candleSeries->sets();

    if (!sets.isEmpty()) {
        QCandlestickSet *last = sets.last();
        if (last->timestamp() == newTime) {
            // Обновляем последнюю свечу
            last->setOpen(decimalToDouble(candle.open()));
            last->setHigh(decimalToDouble(candle.high()));
            last->setLow(decimalToDouble(candle.low()));
            last->setClose(decimalToDouble(candle.close()));
        } else {
            // Новая свеча
            QCandlestickSet *set = candleToSet(candle, m_candleSeries);
            m_candleSeries->append(set);
        }
    } else {
        QCandlestickSet *set = candleToSet(candle, m_candleSeries);
        m_candleSeries->append(set);
    }

    // Расширяем ось X: показываем последний час
    QDateTime newMax = QDateTime::fromMSecsSinceEpoch(newTime);
    m_axisX->setMax(newMax);
    QDateTime windowMin = newMax.addSecs(-3600);
    m_axisX->setMin(windowMin);
    // Подстраиваем ось Y
    qreal high = decimalToDouble(candle.high());
    qreal low  = decimalToDouble(candle.low());
    if (high > m_axisY->max()) m_axisY->setMax(high);
    if (low  < m_axisY->min()) m_axisY->setMin(low);
}

void MainWindow::onLiveCheckBoxToggled(bool checked)
{
    m_liveEnabled = checked;
    if (checked) {
        // Подписаться на стрим свечей для текущего инструмента графика
        if (m_chartInstrumentId != 0)
            m_marketClient->subscribeCandles(m_chartInstrumentId);
    } else {
        // Отписка не предусмотрена в MarketClient, но можно просто перестать обрабатывать сигналы.
        // Для простоты оставим – лишние сообщения будут игнорироваться из-за проверки m_liveEnabled.
    }
}

void MainWindow::onBalanceReceived(const common::Decimal &balance)
{
    ui->current_balance_label->setText(decimalToString(balance));
    // После баланса запрашиваем позиции
    m_accountClient->getPositions();
}

void MainWindow::onPositionsReceived(const account::UserPositions &positions)
{
    const auto &list = positions.positions();
    ui->user_positions_table->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i) {
        const auto &pos = list.at(i);
        // Инструмент
        QString symbol;
        for (const auto &inst : m_cachedInstruments.instruments()) {
            if (inst.id_proto() == pos.instrumentId()) {
                symbol = inst.symbol();
                break;
            }
        }
        if (symbol.isEmpty()) symbol = QString::number(pos.instrumentId());
        ui->user_positions_table->setItem(i, 0, new QTableWidgetItem(symbol));
        ui->user_positions_table->setItem(i, 1, new QTableWidgetItem(decimalToString(pos.quantity())));
        ui->user_positions_table->setItem(i, 2, new QTableWidgetItem(decimalToString(pos.averagePrice())));

        // Расчёт и отображение P&L
        double lastPrice = m_lastPrices.value(pos.instrumentId(), 0.0);
        QString pnlText = "—";
        if (lastPrice != 0.0) {
            double avgPrice = decimalToDouble(pos.averagePrice());
            double qty = decimalToDouble(pos.quantity());
            double pnl = (lastPrice - avgPrice) * qty;
            pnlText = QString::number(pnl, 'f', 8); // или decimalToString, если хотите Decimal обратно
        }
        ui->user_positions_table->setItem(i, 3, new QTableWidgetItem(pnlText));
    }

    m_cachedPositions = positions;
    refreshPositionsTable();

    // Запросим стаканы для всех позиций, чтобы обновить последние цены
    for (const auto &pos : positions.positions()) {
        m_marketClient->getOrderBook(pos.instrumentId());
    }
}

void MainWindow::refreshPositionsTable()
{
    const auto &list = m_cachedPositions.positions();
    ui->user_positions_table->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i) {
        const auto &pos = list.at(i);
        // Инструмент
        QString symbol;
        for (const auto &inst : m_cachedInstruments.instruments()) {
            if (inst.id_proto() == pos.instrumentId()) {
                symbol = inst.symbol();
                break;
            }
        }
        if (symbol.isEmpty()) symbol = QString::number(pos.instrumentId());
        ui->user_positions_table->setItem(i, 0, new QTableWidgetItem(symbol));
        ui->user_positions_table->setItem(i, 1, new QTableWidgetItem(decimalToString(pos.quantity())));
        ui->user_positions_table->setItem(i, 2, new QTableWidgetItem(decimalToString(pos.averagePrice())));

        // P&L
        double lastPrice = m_lastPrices.value(pos.instrumentId(), 0.0);
        double avgPrice = decimalToDouble(pos.averagePrice());
        double qty = decimalToDouble(pos.quantity());
        QString pnlText = "—";
        if (lastPrice != 0.0 && pos.averagePrice().units() > 0) {
            double pnl = (lastPrice - avgPrice) * qty;
            pnlText = QString::number(pnl, 'f', 2);
        }
        ui->user_positions_table->setItem(i, 3, new QTableWidgetItem(pnlText));
    }
}

void MainWindow::onBalanceHistoryReceived(const account::BalanceHistory &history)
{
    ui->balance_history_table->setRowCount(history.entries().size());
    for (int i = 0; i < history.entries().size(); ++i) {
        const auto &entry = history.entries().at(i);
        ui->balance_history_table->setItem(i, 0, new QTableWidgetItem(decimalToString(entry.changeAmount())));
        ui->balance_history_table->setItem(i, 1, new QTableWidgetItem(entry.reason()));
        // время
        QDateTime dt = QDateTime::fromSecsSinceEpoch(entry.timestamp().seconds());
        ui->balance_history_table->setItem(i, 2, new QTableWidgetItem(dt.toString("yyyy-MM-dd hh:mm:ss")));
    }
}

void MainWindow::onDepositClicked()
{
    QString text = ui->money_sum_edit->text();
    QStringList parts = text.split('.');
    int units = parts.value(0).toInt();
    int nanos = 0;
    if (parts.size() > 1) {
        QString frac = parts[1].leftJustified(9, '0');
        nanos = frac.left(9).toInt();
    }
    account::DepositRequest req;
    req.mutAmount().setUnits(units);
    req.mutAmount().setNanos(nanos);
    m_accountClient->deposit(req);
}

void MainWindow::onWithdrawClicked()
{
    QString text = ui->money_sum_edit->text();
    QStringList parts = text.split('.');
    int units = parts.value(0).toInt();
    int nanos = 0;
    if (parts.size() > 1) {
        QString frac = parts[1].leftJustified(9, '0');
        nanos = frac.left(9).toInt();
    }
    account::WithdrawRequest req;
    req.mutAmount().setUnits(units);
    req.mutAmount().setNanos(nanos);
    m_accountClient->withdraw(req);
}

void MainWindow::onDepositSuccess()
{
    // Обновим баланс и историю
    m_accountClient->getBalance();
    m_accountClient->getBalanceHistory();
}

void MainWindow::onWithdrawSuccess()
{
    m_accountClient->getBalance();
    m_accountClient->getBalanceHistory();
}

void MainWindow::onDepositError(const QString &error)
{
    showError("Ошибка депозита:" + error);
}

void MainWindow::onWithdrawError(const QString &error)
{
    showError("Ошибка вывода:" + error);
}

void MainWindow::onPortfolioTabActivated()
{
   m_accountClient->getBalance();
}

void MainWindow::onBalanceError(const QString &error)
{
    showError("Ошибка получения баланса:" + error);
    ui->current_balance_label->setText("Ошибка");
}

void MainWindow::onPositionsError(const QString &error)
{
    showError("Ошибка получения позиций:" + error);
    // Очищаем таблицу или показываем сообщение, если нужно
}

void MainWindow::onBalanceHistoryError(const QString &error)
{
    showError("Ошибка получения истории баланса:" + error);
}

void MainWindow::onAdminTabActivated()
{
    m_adminClient->getSystemStatus();
}

void MainWindow::onUpdateInstrumentClicked()
{
    common::Instrument instr;
    uint64_t id = ui->instrument_id->text().toULongLong();

    // Заполняем поля инструмента
    if (id > 0) instr.setId_proto(id);
    instr.setSymbol(ui->instrument_symbol->text());
    instr.setName(ui->instrument_name->text());

    QStringList tickParts = ui->instrument_tick_size->text().split('.');
    instr.mutTickSize().setUnits(tickParts.value(0).toInt());
    instr.mutTickSize().setNanos(tickParts.size() > 1 ? tickParts[1].leftJustified(9, '0').left(9).toInt() : 0);

    QStringList lotParts = ui->instrument_lot_size->text().split('.');
    instr.mutLotSize().setUnits(lotParts.value(0).toInt());
    instr.mutLotSize().setNanos(lotParts.size() > 1 ? lotParts[1].leftJustified(9, '0').left(9).toInt() : 0);

    instr.setIsActive(ui->instrument_is_active->text().toInt() != 0);

    // Развилка: добавление или обновление
    if (id == 0) {
        m_adminClient->addInstrument(instr);
    } else {
        m_adminClient->updateInstrument(instr);
    }
}

void MainWindow::onInstrumentChanged(int index)
{
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

    // Подписываемся на стримы
    m_marketClient->subscribeOrderBook(instId);
    m_marketClient->subscribeTrades(instId);

    // Запрашиваем мгновенный снапшот стакана
    m_marketClient->getOrderBook(instId);
}

void MainWindow::onAddBalanceClicked()
{
    uint64_t userId = ui->user_id_balance->text().toULongLong();
    QString text = ui->quantity_balance->text();
    QStringList parts = text.split('.');
    common::Decimal amount;
    amount.setUnits(parts.value(0).toInt());
    if (parts.size() > 1) amount.setNanos(parts[1].leftJustified(9, '0').left(9).toInt());
    else amount.setNanos(0);
    m_adminClient->fundUser(userId, amount);
}

void MainWindow::onAddPositionClicked()
{
    uint64_t userId = ui->user_id_positions->text().toULongLong();
    uint64_t instId = ui->instrument_id_positions->currentData().toULongLong();
    QString text = ui->quantity_position->text();
    QStringList parts = text.split('.');
    common::Decimal qty;
    qty.setUnits(parts.value(0).toInt());
    if (parts.size() > 1) qty.setNanos(parts[1].leftJustified(9, '0').left(9).toInt());
    else qty.setNanos(0);
    m_adminClient->addPosition(userId, instId, qty);
}

void MainWindow::onStartStopTradingClicked()
{
    bool newState = !m_adminTradingRunning;
    m_adminClient->setSystemState(newState);
}

void MainWindow::onInstrumentAdded() {
    qDebug() << "Instrument added";
    // Очищаем поля формы
    ui->instrument_id->clear();
    ui->instrument_symbol->clear();
    ui->instrument_name->clear();
    ui->instrument_tick_size->clear();
    ui->instrument_lot_size->clear();
    ui->instrument_is_active->setText("1");
    // Обновим список инструментов (необязательно, но полезно)
    m_adminClient->getSystemStatus();  // если нужно обновить количество активных ордеров и т.д.

    m_marketClient->listInstruments();
}

void MainWindow::onInstrumentUpdated() {
    qDebug() << "Instrument updated";
    // Очистим ID, чтобы можно было добавить новый, не стирая случайно
    ui->instrument_id->clear();
    // При желании также обновить список инструментов в других вкладках
    m_adminClient->getSystemStatus();

    m_marketClient->listInstruments();
}

void MainWindow::onInstrumentError(const QString &error) { showError("Ошибка получения инструмента:" + error); }
void MainWindow::onFundSuccess()
{
    ui->fund_status_label->setText("Balance updated successfully");
    ui->fund_status_label->setStyleSheet("color: green; font-weight: bold;");
    // Через 3 секунды очистим сообщение
    QTimer::singleShot(3000, this, [this]() {
        ui->fund_status_label->clear();
        ui->fund_status_label->setStyleSheet("");
    });
}

void MainWindow::onFundError(const QString &error)
{
    ui->fund_status_label->setText("Error: " + error);
    ui->fund_status_label->setStyleSheet("color: red; font-weight: bold;");
    QTimer::singleShot(5000, this, [this]() {
        ui->fund_status_label->clear();
        ui->fund_status_label->setStyleSheet("");
    });
}

void MainWindow::onPositionAdded()
{
    ui->position_status_label->setText("Position added successfully");
    ui->position_status_label->setStyleSheet("color: green; font-weight: bold;");
    QTimer::singleShot(3000, this, [this]() {
        ui->position_status_label->clear();
        ui->position_status_label->setStyleSheet("");
    });
}

void MainWindow::onPositionError(const QString &error)
{
    ui->position_status_label->setText("Error: " + error);
    ui->position_status_label->setStyleSheet("color: red; font-weight: bold;");
    QTimer::singleShot(5000, this, [this]() {
        ui->position_status_label->clear();
        ui->position_status_label->setStyleSheet("");
    });
}

void MainWindow::onSystemStatusReceived(const admin::SystemStatus &status)
{
    ui->trading_status_label->setText(status.isRunning() ? "Работает" : "Остановлен");
    ui->active_orders_label->setText(QString::number(status.activeOrdersCount()));
    ui->users_count_label->setText(QString::number(status.totalUsersCount()));
    ui->system_version_label->setText(status.serverVersion().isEmpty() ? "1.0.0-rc1" : status.serverVersion());
    ui->uptime_label->setText(status.uptime().isEmpty() ? "0 часов" : status.uptime());

    m_adminTradingRunning = status.isRunning();
    ui->start_stop_trading_button->setText(status.isRunning() ? "Остановить торги" : "Начать торги");
}

void MainWindow::onSystemStatusError(const QString &error) { showError("Ошибка получения статуса системы:" + error); }
void MainWindow::onSystemStateChanged(bool running)
{
    m_adminClient->getSystemStatus(); // обновим метки
}
void MainWindow::onSystemStateError(const QString &error) { showError("Ошибка получения состояния системы:" + error); }

void MainWindow::onAdminInstrumentSelected(int index)
{
    if (index < 0) return;
    uint64_t id = ui->instrument_select_admin->currentData().toULongLong();

    ui->instrument_id->setText(id > 0 ? QString::number(id) : QString());

    if (id == 0) {
        // Очищаем поля для нового инструмента
        ui->instrument_symbol->clear();
        ui->instrument_name->clear();
        ui->instrument_tick_size->clear();
        ui->instrument_lot_size->clear();
        ui->instrument_is_active->setText("1");   // по умолчанию активен
        return;
    }

    // Ищем инструмент в кэше
    for (const auto &inst : m_cachedInstruments.instruments()) {
        if (inst.id_proto() == id) {
            ui->instrument_symbol->setText(inst.symbol());
            ui->instrument_name->setText(inst.name());
            ui->instrument_tick_size->setText(decimalToString(inst.tickSize()));
            ui->instrument_lot_size->setText(decimalToString(inst.lotSize()));
            ui->instrument_is_active->setText(inst.isActive() ? "1" : "0");
            return;
        }
    }
}

void MainWindow::onDeleteInstrumentClicked() {
    uint64_t id = ui->instrument_id->text().toULongLong();
    if (id == 0) {
        QMessageBox::warning(this, "Ошибка", "Введите ID инструмента");
        return;
    }
    m_adminClient->deleteInstrument(id);
}

void MainWindow::onUsersListed(const admin::ListUsersResponse &users) {
    ui->users_table->setRowCount(users.users().size());
    for (int i = 0; i < users.users().size(); ++i) {
        const auto &u = users.users().at(i);
        ui->users_table->setItem(i, 0, new QTableWidgetItem(QString::number(u.id_proto())));
        ui->users_table->setItem(i, 1, new QTableWidgetItem(u.username()));
        ui->users_table->setItem(i, 2, new QTableWidgetItem(u.email()));
        ui->users_table->setItem(i, 3, new QTableWidgetItem(u.role() == auth::User::Role::ADMIN ? "ADMIN" : "USER"));
        ui->users_table->setItem(i, 4, new QTableWidgetItem(u.isActive() ? "Да" : "Нет"));
    }
}

void MainWindow::onApplyUserChanges() {
    int row = ui->users_table->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "Ошибка", "Выберите пользователя в таблице");
        return;
    }
    uint64_t userId = ui->users_table->item(row, 0)->text().toULongLong();
    auth::User::Role newRole = (ui->user_role_combo->currentText() == "ADMIN")
                                   ? auth::User::Role::ADMIN : auth::User::Role::USER;
    bool active = ui->user_active_check->isChecked();

    // Применяем оба изменения
    m_adminClient->setUserRole(userId, newRole);
    m_adminClient->setUserActive(userId, active);
}

