#pragma once

#include <QImage>

#include <utility>

class QClipboard;

namespace patchy::ui {

// Reads the highest-resolution raster representation offered by the system
// clipboard. macOS can expose both a full-size image and a display thumbnail;
// choosing by pixel area keeps paste/new-document operations at source quality.
[[nodiscard]] QImage image_from_clipboard(const QClipboard* clipboard);

// Returns the image's embedded density. Clipboard images without an explicit
// density use Photoshop's 72 PPI convention on both axes.
[[nodiscard]] std::pair<double, double> clipboard_image_ppi(const QImage& image) noexcept;

// Stores document density in the QImage metadata before it is published to the
// system clipboard, so a later paste/new-document operation can recover it.
void set_clipboard_image_ppi(QImage& image, double horizontal_ppi, double vertical_ppi) noexcept;

}  // namespace patchy::ui
