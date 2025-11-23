// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2014-2020 The Dash Core developers
// Copyright (c) 2025 The 405Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/405Coin-config.h>
#endif

#include <qt/splashscreen.h>

#include <algorithm>
#include <array>
#include <chainparams.h>
#include <clientversion.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <qt/guiutil.h>
#include <qt/networkstyle.h>
#include <qt/walletmodel.h>
#include <ui_interface.h>
#include <util/system.h>

#include <functional>

#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QRandomGenerator>
#include <QScreen>
#include <QRadialGradient>
#include <QTimer>

namespace pl = std::placeholders;

namespace {
constexpr int kSplashWidth = 480;
constexpr int kSplashHeight = 540;
constexpr int kMethodDurationDefault = 1200;
constexpr int kGlitchDurationDefault = 1500;
constexpr int kRevealDurationDefault = 1200;
constexpr int kMinimumDisplayPaddingMs = 400;
}

SplashScreen::SplashScreen(interfaces::Node &node, Qt::WindowFlags f, const NetworkStyle *networkStyle) :
        QWidget(nullptr, f),
        curAlignment(0),
        memePhase(MemePhase::MethodBanner),
        revealProgress(0.0),
        animationTimer(nullptr),
        finishTimer(nullptr),
        methodDurationMs(kMethodDurationDefault),
        glitchDurationMs(kGlitchDurationDefault),
        revealDurationMs(kRevealDurationDefault),
        minimumDisplayMs(kMethodDurationDefault + kGlitchDurationDefault + kRevealDurationDefault + kMinimumDisplayPaddingMs),
        hasFinished(false),
        m_node(node) {

    // transparent background
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background:transparent;");

    // no window decorations
    setWindowFlags(Qt::FramelessWindowHint);

    // Geometries of splashscreen
    const int width = kSplashWidth;
    const int height = kSplashHeight;
    const int paddingTop = 14;
    const int titleVersionVSpace = 54;

    qreal scale = qApp->devicePixelRatio();

    // define text to place
    const QString titleText = PACKAGE_NAME;
    const QString versionText = QString::fromStdString(FormatFullVersion()).remove(0, 1);
    const QString titleAddText = networkStyle->getTitleAddText();

    QFont fontNormal = GUIUtil::getFontNormal();
    QFont fontBold = GUIUtil::getFontBold();
    qreal fontFactor = 1.0;

    pixmap = QPixmap(int(width * scale), int(height * scale));
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(GUIUtil::getThemedQColor(GUIUtil::ThemedColor::BORDER_WIDGET));

    QPainter pixPaint(&pixmap);
    QRect rect = QRect(1, 1, width - 2, height - 2);
    QColor background = GUIUtil::getThemedQColor(GUIUtil::ThemedColor::BACKGROUND_WIDGET);
    pixPaint.fillRect(rect, background);

    QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
    gradient.setColorAt(0.0, background.lighter(108));
    gradient.setColorAt(1.0, background.darker(110));
    pixPaint.setOpacity(0.9);
    pixPaint.fillRect(rect, gradient);
    pixPaint.setOpacity(1.0);

    // subtle scanline grid to sell the glitch aesthetic
    QPen gridPen(GUIUtil::getThemedQColor(GUIUtil::ThemedColor::BORDER_NETSTATS));
    gridPen.setStyle(Qt::DotLine);
    gridPen.setWidth(1);
    pixPaint.setPen(gridPen);
    for (int y = rect.top() + 90; y < rect.bottom() - 90; y += 60) {
        pixPaint.drawLine(rect.left() + 24, y, rect.right() - 24, y);
    }

    pixPaint.setPen(GUIUtil::getThemedQColor(GUIUtil::ThemedColor::DEFAULT));
    fontBold.setPointSize(30 * fontFactor);
    pixPaint.setFont(fontBold);
    QFontMetrics fm = pixPaint.fontMetrics();
    int titleTextWidth = GUIUtil::TextWidth(fm, titleText);
    if (titleTextWidth > width * 0.8) {
        fontFactor = 0.85;
        fontBold.setPointSize(30 * fontFactor);
        pixPaint.setFont(fontBold);
    }
    pixPaint.drawText(QRect(0, paddingTop, width, 40), Qt::AlignHCenter | Qt::AlignTop, titleText);

    fontNormal.setPointSize(16 * fontFactor);
    pixPaint.setFont(fontNormal);
    pixPaint.setPen(GUIUtil::getThemedQColor(GUIUtil::ThemedColor::SECONDARY));
    pixPaint.drawText(QRect(0, paddingTop + titleVersionVSpace, width, 30), Qt::AlignHCenter | Qt::AlignTop,
                      versionText);

    // draw additional text if special network
    if (!titleAddText.isEmpty()) {
        fontBold.setPointSize(20);
        pixPaint.setFont(fontBold);
        fm = pixPaint.fontMetrics();
        int titleAddTextWidth = GUIUtil::TextWidth(fm, titleAddText);
        QRect badgeRect = QRect(rect.center().x() - ((titleAddTextWidth + 20) / 2), paddingTop - 6,
                                titleAddTextWidth + 20, fm.height() + 12);
        QColor badgeColor = networkStyle->getBadgeColor();
        pixPaint.fillRect(badgeRect, badgeColor);
        pixPaint.setPen(Qt::white);
        pixPaint.drawText(badgeRect, Qt::AlignCenter, titleAddText);
    }

    pixPaint.end();

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), QSize(width, height));
    resize(r.size());
    setFixedSize(r.size());
    move(QGuiApplication::primaryScreen()->geometry().center() - r.center());

    animationClock.start();
    updateMemePhase();
    animationTimer = new QTimer(this);
    animationTimer->setInterval(70);
    QObject::connect(animationTimer, &QTimer::timeout, this, [this]() {
        updateMemePhase();
        update();
    });
    animationTimer->start();

    finishTimer = new QTimer(this);
    finishTimer->setSingleShot(true);
    QObject::connect(finishTimer, &QTimer::timeout, this, [this]() {
        completeFinish();
    });

    subscribeToCoreSignals();
    installEventFilter(this);
}

SplashScreen::~SplashScreen() {
    if (animationTimer) {
        animationTimer->stop();
    }
    if (finishTimer) {
        finishTimer->stop();
    }
    unsubscribeFromCoreSignals();
}

bool SplashScreen::eventFilter(QObject *obj, QEvent *ev) {
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if (keyEvent->text()[0] == 'q') {
            m_node.startShutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

void SplashScreen::finish() {
    if (hasFinished) {
        return;
    }
    qint64 elapsed = animationClock.isValid() ? animationClock.elapsed() : minimumDisplayMs;
    qint64 remaining = minimumDisplayMs - elapsed;
    if (remaining > 0 && finishTimer) {
        if (!finishTimer->isActive()) {
            finishTimer->start(static_cast<int>(remaining));
        }
        return;
    }
    completeFinish();
}

static void InitMessage(SplashScreen *splash, const std::string &message) {
    bool invoked = QMetaObject::invokeMethod(splash, "showMessage",
                                             Qt::QueuedConnection,
                                             Q_ARG(QString, QString::fromStdString(message)),
                                             Q_ARG(int, Qt::AlignBottom | Qt::AlignHCenter),
                                             Q_ARG(QColor, GUIUtil::getThemedQColor(GUIUtil::ThemedColor::DEFAULT)));
    assert(invoked);
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress, bool resume_possible) {
    InitMessage(splash, title + std::string("\n") +
                        (resume_possible ? _("(press q to shutdown and continue later)")
                                         : _("press q to shutdown")) +
                        strprintf("\n%d", nProgress) + "%");
}

void SplashScreen::subscribeToCoreSignals() {
    // Connect signals to client
    m_handler_init_message = m_node.handleInitMessage(std::bind(InitMessage, this, pl::_1));
    m_handler_show_progress = m_node.handleShowProgress(std::bind(ShowProgress, this, pl::_1, pl::_2, pl::_3));
}

void SplashScreen::handleLoadWallet() {
#ifdef ENABLE_WALLET
    if (!WalletModel::isWalletEnabled()) return;
    m_handler_load_wallet = m_node.walletClient().handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) {
        m_connected_wallet_handlers.emplace_back(wallet->handleShowProgress(std::bind(ShowProgress, this, pl::_1, pl::_2, false)));
        m_connected_wallets.emplace_back(std::move(wallet));
    });
#endif
}

void SplashScreen::unsubscribeFromCoreSignals() {
    // Disconnect signals from client
    m_handler_init_message->disconnect();
    m_handler_show_progress->disconnect();
#ifdef ENABLE_WALLET
    if (m_handler_load_wallet != nullptr) {
        m_handler_load_wallet->disconnect();
    }
#endif // ENABLE_WALLET
    for (const auto &handler: m_connected_wallet_handlers) {
        handler->disconnect();
    }
    m_connected_wallet_handlers.clear();
    m_connected_wallets.clear();
}

void SplashScreen::renderMemeBoot(QPainter &painter) {
    painter.save();
    painter.setClipRect(rect().adjusted(8, 80, -8, -60));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    switch (memePhase) {
        case MemePhase::MethodBanner:
            drawMethodBanner(painter, 1.0, false);
            break;
        case MemePhase::Glitching:
            drawMethodBanner(painter, 1.0, true);
            drawGlitchBursts(painter, 1.0);
            break;
        case MemePhase::LogoReveal:
            drawMethodBanner(painter, std::max(0.0, 1.0 - revealProgress), true);
            drawLogoReveal(painter, revealProgress);
            drawGlitchBursts(painter, std::max(0.0, 0.8 - revealProgress));
            break;
        case MemePhase::Settled:
            drawLogoReveal(painter, 1.0);
            break;
    }

    painter.restore();
}

void SplashScreen::drawMethodBanner(QPainter &painter, qreal opacity, bool glitchy) {
    if (opacity <= 0.0) return;

    qreal clampedOpacity = opacity;
    if (clampedOpacity > 1.0) clampedOpacity = 1.0;
    if (clampedOpacity < 0.0) clampedOpacity = 0.0;

    painter.save();
    painter.setOpacity(clampedOpacity);

    QFont titleFont = GUIUtil::getFontBold();
    titleFont.setPointSize(32);
    painter.setFont(titleFont);

    QRect bannerRect = rect().adjusted(10, 130, -10, -240);
    const QString methodText = tr("Method Not Allowed");
    QRandomGenerator *gen = QRandomGenerator::global();

    if (!glitchy) {
        painter.setPen(GUIUtil::getThemedQColor(GUIUtil::ThemedColor::DEFAULT));
        painter.drawText(bannerRect, Qt::AlignCenter, methodText);
    } else {
        static const std::array<QColor, 3> glitchPalette{
                QColor("#ff3cac"),
                QColor("#40c9ff"),
                QColor("#f9ff21")
        };
        painter.setPen(GUIUtil::getThemedQColor(GUIUtil::ThemedColor::DEFAULT));
        painter.drawText(bannerRect, Qt::AlignCenter, methodText);
        auto jitter = [gen](int magnitude) {
            if (magnitude <= 0) return 0;
            int value = gen->bounded(magnitude + 1);
            return (gen->bounded(2) == 0) ? value : -value;
        };
        for (const QColor &color: glitchPalette) {
            painter.save();
            QColor ghost = color;
            ghost.setAlpha(180);
            painter.setPen(ghost);
            QRect jitterRect = bannerRect.translated(jitter(16), jitter(10));
            painter.drawText(jitterRect, Qt::AlignCenter, methodText);
            painter.restore();
        }
    }

    QFont subFont = GUIUtil::getFontNormal();
    subFont.setPointSize(14);
    painter.setFont(subFont);
    painter.setPen(GUIUtil::getThemedQColor(GUIUtil::ThemedColor::SECONDARY));
    QRect subRect = bannerRect.adjusted(0, 90, 0, 0);
    painter.drawText(subRect, Qt::AlignHCenter | Qt::AlignTop, tr("Meme Boot • glitching reality…"));

    painter.restore();
}

void SplashScreen::drawGlitchBursts(QPainter &painter, qreal intensity) {
    if (intensity <= 0.0) return;

    QRect area = rect().adjusted(24, 150, -24, -130);
    QRandomGenerator *gen = QRandomGenerator::global();
    int slices = 6 + gen->bounded(6);

    for (int i = 0; i < slices; ++i) {
        int sliceHeight = 4 + gen->bounded(18);
        int heightRange = area.height() - sliceHeight;
        if (heightRange <= 0) continue;
        int y = area.top() + gen->bounded(heightRange);
        int sliceWidth = area.width();
        if (sliceWidth <= 0) continue;
        int shrink = gen->bounded(std::min(sliceWidth, 40));
        int width = std::max(8, sliceWidth - shrink);
        int xOffset = gen->bounded(2) == 0 ? -gen->bounded(20) : gen->bounded(20);
        QColor color = (i % 2 == 0) ? QColor("#f72585") : QColor("#22d3ee");
        int alpha = static_cast<int>((120 + gen->bounded(80)) * intensity);
        color.setAlpha(alpha);
        QRect slice(area.left() + xOffset, y, width, sliceHeight);
        painter.fillRect(slice, color);
    }
}

void SplashScreen::drawLogoReveal(QPainter &painter, qreal opacity) {
    if (opacity <= 0.0) {
        return;
    }
    qreal clampedOpacity = std::clamp(opacity, 0.0, 1.0);

    const QColor accentTeal(64, 199, 196);
    const QColor accentViolet(166, 129, 212);
    QRectF haloRect(width() / 2.0 - 150, height() / 2.0 - 140, 300, 300);

    QRadialGradient haloGradient(haloRect.center(), haloRect.width() / 2.0);
    haloGradient.setColorAt(0.0, QColor(accentTeal.red(), accentTeal.green(), accentTeal.blue(), int(210 * clampedOpacity)));
    haloGradient.setColorAt(0.55, QColor(accentViolet.red(), accentViolet.green(), accentViolet.blue(), int(170 * clampedOpacity)));
    haloGradient.setColorAt(1.0, QColor(10, 16, 24, 0));

    painter.save();
    painter.setOpacity(0.85);
    painter.setPen(Qt::NoPen);
    painter.setBrush(haloGradient);
    painter.drawEllipse(haloRect);
    painter.restore();

    painter.save();
    painter.setOpacity(0.7 * clampedOpacity);
    QPen outerPen(accentViolet);
    outerPen.setWidthF(2.0);
    outerPen.setCapStyle(Qt::RoundCap);
    painter.setPen(outerPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(haloRect.adjusted(10, 10, -10, -10), int(-50 * 16), int(230 * 16 * clampedOpacity));
    QPen innerPen(accentTeal);
    innerPen.setWidthF(3.0);
    innerPen.setCapStyle(Qt::RoundCap);
    painter.setPen(innerPen);
    painter.drawArc(haloRect.adjusted(28, 28, -28, -28), int(110 * 16), int(260 * 16 * clampedOpacity));
    painter.restore();

    painter.save();
    painter.setOpacity(0.35 * clampedOpacity);
    for (int i = 0; i < 5; ++i) {
        qreal widthScale = 0.6 + (0.08 * i);
        QRectF barRect(QPointF(width() * (0.5 - widthScale / 2.0), haloRect.top() + 40 + i * 28),
                       QSizeF(width() * widthScale, 6));
        QLinearGradient barGradient(barRect.topLeft(), barRect.topRight());
        barGradient.setColorAt(0.0, accentTeal);
        barGradient.setColorAt(1.0, accentViolet);
        painter.fillRect(barRect, barGradient);
    }
    painter.restore();

    painter.save();
    painter.setOpacity(clampedOpacity);
    QFont captionFont = GUIUtil::getFontBold();
    captionFont.setPointSize(20);
    painter.setFont(captionFont);
    painter.setPen(GUIUtil::getThemedQColor(GUIUtil::ThemedColor::DEFAULT));
    painter.drawText(QRect(0, height() - 190, width(), 40), Qt::AlignHCenter | Qt::AlignVCenter,
                     tr("405COIN MEME BOOT"));

    QFont subFont = GUIUtil::getFontNormal();
    subFont.setPointSize(13);
    painter.setFont(subFont);
    painter.setPen(GUIUtil::getThemedQColor(GUIUtil::ThemedColor::SECONDARY));
    painter.drawText(QRect(0, height() - 155, width(), 30), Qt::AlignHCenter | Qt::AlignTop,
                     tr("Method restored · Please stand by"));
    painter.restore();
}

void SplashScreen::updateMemePhase() {
    if (!animationClock.isValid()) {
        animationClock.start();
    }

    const qint64 elapsed = animationClock.elapsed();
    const qint64 glitchStart = methodDurationMs;
    const qint64 revealStart = glitchStart + glitchDurationMs;
    const qint64 animationEnd = revealStart + revealDurationMs;

    if (elapsed < methodDurationMs) {
        memePhase = MemePhase::MethodBanner;
        revealProgress = 0.0;
        return;
    }
    if (elapsed < revealStart) {
        memePhase = MemePhase::Glitching;
        revealProgress = 0.0;
        return;
    }
    if (elapsed < animationEnd) {
        memePhase = MemePhase::LogoReveal;
        qreal local = qreal(elapsed - revealStart) / qreal(std::max(1, revealDurationMs));
        if (local < 0.0) local = 0.0;
        if (local > 1.0) local = 1.0;
        revealProgress = local;
        return;
    }

    memePhase = MemePhase::Settled;
    revealProgress = 1.0;
    if (animationTimer && animationTimer->isActive()) {
        animationTimer->stop();
    }
}

void SplashScreen::completeFinish() {
    if (hasFinished) {
        return;
    }
    hasFinished = true;
    if (finishTimer && finishTimer->isActive()) {
        finishTimer->stop();
    }
    if (animationTimer && animationTimer->isActive()) {
        animationTimer->stop();
    }
    if (isMinimized()) {
        showNormal();
    }
    Q_EMIT readyToLaunch();
    hide();
    deleteLater();
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color) {
    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    QFont messageFont = GUIUtil::getFontNormal();
    messageFont.setPointSize(14);
    painter.setFont(messageFont);
    painter.drawPixmap(0, 0, pixmap);
    renderMemeBoot(painter);
    QRect r = rect().adjusted(5, 5, -5, -15);
    painter.setPen(curColor);
    painter.drawText(r, curAlignment, curMessage);
}

void SplashScreen::closeEvent(QCloseEvent *event) {
    m_node.startShutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
