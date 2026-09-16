#include "ui/app_credits.hpp"

namespace patchy::ui {

QString my_aips_project_link_html(const QString& link_color) {
  return QStringLiteral("<a style=\"color:%1; text-decoration:none;\" "
                        "href=\"https://github.com/ddxmu/MyAIPs\">ddxmu/MyAIPs</a>")
      .arg(link_color);
}

}  // namespace patchy::ui
