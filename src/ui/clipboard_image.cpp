#include "ui/clipboard_image.hpp"

#include <QBuffer>
#include <QClipboard>
#include <QIODevice>
#include <QImageReader>
#include <QMimeData>

#include <algorithm>
#include <cmath>
#include <limits>

namespace patchy::ui {

namespace {

constexpr double kClipboardFallbackPpi = 72.0;
constexpr double kMetersPerInch = 0.0254;

std::pair<int, int> default_dots_per_meter() {
  static const auto defaults = [] {
    const QImage reference(1, 1, QImage::Format_ARGB32);
    return std::pair<int, int>(reference.dotsPerMeterX(), reference.dotsPerMeterY());
  }();
  return defaults;
}

double ppi_from_dots_per_meter(int dots_per_meter, int default_dots) noexcept {
  if (dots_per_meter <= 0 || dots_per_meter == default_dots) {
    return kClipboardFallbackPpi;
  }
  return std::clamp(static_cast<double>(dots_per_meter) * kMetersPerInch, 0.01, 9999.0);
}

int dots_per_meter_from_ppi(double ppi) noexcept {
  const auto sanitized = std::isfinite(ppi) && ppi > 0.0 ? std::clamp(ppi, 0.01, 9999.0)
                                                          : kClipboardFallbackPpi;
  return std::clamp(static_cast<int>(std::lround(sanitized / kMetersPerInch)), 1, 1000000);
}

bool is_raster_image_mime(const QString& format) {
  const auto lower = format.toLower();
  return lower == QStringLiteral("image/png") || lower == QStringLiteral("image/tiff") ||
         lower == QStringLiteral("image/bmp") || lower == QStringLiteral("image/jpeg") ||
         lower == QStringLiteral("image/webp") || lower == QStringLiteral("public.png") ||
         lower == QStringLiteral("public.tiff") || lower == QStringLiteral("public.jpeg") ||
         lower == QStringLiteral("public.bmp");
}

QImage decode_image_bytes(const QByteArray& bytes) {
  if (bytes.isEmpty()) {
    return {};
  }
  QBuffer buffer;
  buffer.setData(bytes);
  if (!buffer.open(QIODevice::ReadOnly)) {
    return {};
  }
  QImageReader reader(&buffer);
  reader.setAutoTransform(true);
  return reader.read();
}

bool is_better_candidate(const QImage& candidate, const QImage& best, int candidate_priority,
                         int best_priority) {
  if (candidate.isNull()) {
    return false;
  }
  if (best.isNull()) {
    return true;
  }
  const auto candidate_area = static_cast<qint64>(candidate.width()) * candidate.height();
  const auto best_area = static_cast<qint64>(best.width()) * best.height();
  return candidate_area > best_area || (candidate_area == best_area && candidate_priority < best_priority);
}

}  // namespace

QImage image_from_clipboard(const QClipboard* clipboard) {
  if (clipboard == nullptr) {
    return {};
  }

  QImage best;
  int best_priority = std::numeric_limits<int>::max();
  const auto* mime = clipboard->mimeData();
  if (mime != nullptr) {
    int priority = 0;
    for (const auto& format : mime->formats()) {
      if (!is_raster_image_mime(format)) {
        continue;
      }
      const auto candidate = decode_image_bytes(mime->data(format));
      if (is_better_candidate(candidate, best, priority, best_priority)) {
        best = candidate;
        best_priority = priority;
      }
      ++priority;
    }
  }

  // QClipboard::image() is the fallback for native clipboard providers that
  // expose an image variant without a byte-oriented MIME representation.
  const auto native_image = clipboard->image();
  if (is_better_candidate(native_image, best, 0, best_priority)) {
    best = native_image;
  }
  return best;
}

std::pair<double, double> clipboard_image_ppi(const QImage& image) noexcept {
  const auto defaults = default_dots_per_meter();
  if (image.isNull()) {
    return {kClipboardFallbackPpi, kClipboardFallbackPpi};
  }
  return {ppi_from_dots_per_meter(image.dotsPerMeterX(), defaults.first),
          ppi_from_dots_per_meter(image.dotsPerMeterY(), defaults.second)};
}

void set_clipboard_image_ppi(QImage& image, double horizontal_ppi, double vertical_ppi) noexcept {
  if (image.isNull()) {
    return;
  }
  image.setDotsPerMeterX(dots_per_meter_from_ppi(horizontal_ppi));
  image.setDotsPerMeterY(dots_per_meter_from_ppi(vertical_ppi));
}

}  // namespace patchy::ui
