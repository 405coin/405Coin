// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2025 The 405Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SPLASHSCREEN_H
#define BITCOIN_QT_SPLASHSCREEN_H

#include <QElapsedTimer>
#include <QPixmap>
#include <QWidget>

#include <memory>

class QTimer;
class NetworkStyle;

namespace interfaces {
    class Handler;

    class Node;

    class Wallet;
};

/** Class for the splashscreen with information of the running client.
 *
 * @note this is intentionally not a QSplashScreen. 405Coin Core initialization
 * can take a long time, and in that case a progress window that cannot be
 * moved around and minimized has turned out to be frustrating to the user.
 */
class SplashScreen : public QWidget {
    Q_OBJECT

public:
    explicit SplashScreen(interfaces::Node &node, Qt::WindowFlags f, const NetworkStyle *networkStyle);

    ~SplashScreen();

protected:
    void paintEvent(QPaintEvent *event) override;

    void closeEvent(QCloseEvent *event) override;

public
    Q_SLOTS:
            /** Hide the splash screen window and schedule the splash screen object for deletion */
            void finish();

    /** Show message and progress */
    void showMessage(const QString &message, int alignment, const QColor &color);

    void handleLoadWallet();

Q_SIGNALS:
    void readyToLaunch();

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    enum class MemePhase {
        MethodBanner,
        Glitching,
        LogoReveal,
        Settled
    };

    /** Connect core signals to splash screen */
    void subscribeToCoreSignals();

    /** Disconnect core signals to splash screen */
    void unsubscribeFromCoreSignals();

    void renderMemeBoot(QPainter &painter);
    void drawMethodBanner(QPainter &painter, qreal opacity, bool glitchy);
    void drawGlitchBursts(QPainter &painter, qreal intensity);
    void drawLogoReveal(QPainter &painter, qreal opacity);
    void updateMemePhase();
    void completeFinish();

    QPixmap pixmap;
    QString curMessage;
    QColor curColor;
    int curAlignment;
    MemePhase memePhase;
    qreal revealProgress;
    QElapsedTimer animationClock;
    QTimer *animationTimer;
    QTimer *finishTimer;
    int methodDurationMs;
    int glitchDurationMs;
    int revealDurationMs;
    int minimumDisplayMs;
    bool hasFinished;

    interfaces::Node &m_node;
    std::unique_ptr <interfaces::Handler> m_handler_init_message;
    std::unique_ptr <interfaces::Handler> m_handler_show_progress;
    std::unique_ptr <interfaces::Handler> m_handler_load_wallet;
    std::list <std::unique_ptr<interfaces::Wallet>> m_connected_wallets;
    std::list <std::unique_ptr<interfaces::Handler>> m_connected_wallet_handlers;
};

#endif // BITCOIN_QT_SPLASHSCREEN_H
