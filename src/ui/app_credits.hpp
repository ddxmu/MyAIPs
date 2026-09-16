#pragma once

#include <QString>

namespace patchy::ui {

// Rich-text project link used by the About dialog and the start panel; link_color
// is the anchor color for the site.
[[nodiscard]] QString my_aips_project_link_html(const QString& link_color);

}  // namespace patchy::ui
