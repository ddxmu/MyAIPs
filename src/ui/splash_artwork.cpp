#include "ui/splash_artwork.hpp"

#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include <algorithm>

namespace patchy::ui {

namespace {

// The artwork was designed at the About dialog's 210x270; painting happens in that
// logical space and scales uniformly so other sizes keep the same proportions.
constexpr int kLogicalWidth = 210;
constexpr int kLogicalHeight = 270;

}  // namespace

SplashArtwork::SplashArtwork(QWidget* parent) : QWidget(parent) {}

void SplashArtwork::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const qreal scale = std::min(width() / qreal(kLogicalWidth), height() / qreal(kLogicalHeight));
  painter.translate((width() - kLogicalWidth * scale) / 2.0, (height() - kLogicalHeight * scale) / 2.0);
  painter.scale(scale, scale);

  const QRectF icon(34, 58, 142, 142);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(20, 19, 65, 62));
  painter.drawRoundedRect(icon.translated(0, 7), 30, 30);

  QLinearGradient gradient(icon.topLeft(), icon.bottomRight());
  gradient.setColorAt(0.0, QColor(40, 112, 250));
  gradient.setColorAt(0.52, QColor(100, 76, 243));
  gradient.setColorAt(1.0, QColor(172, 54, 220));
  painter.setBrush(gradient);
  painter.setPen(QPen(QColor(240, 244, 255, 92), 1.5));
  painter.drawRoundedRect(icon, 30, 30);

  QFont lettermark(QStringLiteral("Avenir Next"));
  lettermark.setPixelSize(69);
  lettermark.setWeight(QFont::Bold);
  lettermark.setLetterSpacing(QFont::PercentageSpacing, 94);
  painter.setFont(lettermark);
  painter.setPen(QColor(247, 249, 255));
  painter.drawText(icon.adjusted(2, 0, -2, 1), Qt::AlignCenter, QStringLiteral("PS"));
}

}  // namespace patchy::ui
