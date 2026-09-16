// Translation catalog checks (docs/localization.md).
//
// - translation_template_is_current reruns lupdate with the manifest CMake wrote
//   (same binary, options and source list as the patchy_update_translations target)
//   and fails when translations/patchy_en.ts does not match the code, i.e. when a
//   string change landed without scripts\update-translations.ps1.
// - translation_catalogs_are_complete requires every shipped language to carry the
//   template's exact string set with no unfinished entries and with placeholders,
//   modifier tokens, accelerators and trailing punctuation preserved.
// - translation_template_covers_runtime_sources walks the strings that reach the UI
//   outside lupdate's view (bound properties, tooltip details, preset tables) and
//   fails when one is missing from the template.

#include "test_harness.hpp"
#include "ui_test_groups.hpp"
#include "ui_test_support.hpp"

#include "core/contour_presets.hpp"
#include "core/gradient_presets.hpp"
#include "core/palette_presets.hpp"
#include "core/pattern_presets.hpp"
#include "core/style_presets.hpp"
#include "ui/default_custom_shapes.hpp"
#include "ui/hotkey_registry.hpp"
#include "ui/localization.hpp"
#include "ui/main_window.hpp"
#include "ui/main_window_shared.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QXmlStreamReader>

#include <cstdio>
#include <vector>

namespace {

using namespace patchy::test::ui;

struct CatalogMessage {
  QString context;
  QString source;
  QString comment;  // lupdate disambiguation comment
  bool numerus{false};
  QString type;  // translation "type" attribute: empty, unfinished, vanished, obsolete
  QStringList translations;  // one entry, or one per plural form
};

struct Catalog {
  QString language;
  QString source_language;
  std::vector<CatalogMessage> messages;
};

QString message_key(const CatalogMessage& message) {
  return message.context + QLatin1Char('\x1f') + message.source + QLatin1Char('\x1f') + message.comment;
}

QString describe(const CatalogMessage& message) {
  auto text = QStringLiteral("[%1] %2").arg(message.context, message.source.left(80));
  if (!message.comment.isEmpty()) {
    text += QStringLiteral(" (%1)").arg(message.comment);
  }
  return text.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
}

Catalog read_catalog(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    std::fprintf(stderr, "cannot open catalog %s\n", qPrintable(path));
    CHECK(false);
  }
  QXmlStreamReader xml(&file);
  Catalog catalog;
  QString context;
  CatalogMessage current;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement()) {
      const auto name = xml.name();
      if (name == u"TS") {
        catalog.language = xml.attributes().value(u"language").toString();
        catalog.source_language = xml.attributes().value(u"sourcelanguage").toString();
      } else if (name == u"name") {
        context = xml.readElementText();
      } else if (name == u"message") {
        current = CatalogMessage{};
        current.context = context;
        current.numerus = xml.attributes().value(u"numerus") == u"yes";
      } else if (name == u"source") {
        current.source = xml.readElementText();
      } else if (name == u"comment") {
        current.comment = xml.readElementText();
      } else if (name == u"translation") {
        current.type = xml.attributes().value(u"type").toString();
        if (current.numerus) {
          while (!xml.atEnd() && !(xml.isEndElement() && xml.name() == u"translation")) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == u"numerusform") {
              current.translations.push_back(xml.readElementText());
            }
          }
        } else {
          current.translations.push_back(xml.readElementText());
        }
      }
    } else if (xml.isEndElement() && xml.name() == u"message") {
      catalog.messages.push_back(current);
    }
  }
  if (xml.hasError()) {
    std::fprintf(stderr, "%s: %s\n", qPrintable(path), qPrintable(xml.errorString()));
    CHECK(false);
  }
  return catalog;
}

struct LupdateManifest {
  QString lupdate;
  QStringList options;
  QString include;
  QString template_path;
  QStringList languages;
  QStringList sources;
};

LupdateManifest read_manifest() {
  LupdateManifest manifest;
  QFile file(QStringLiteral(PATCHY_LUPDATE_MANIFEST));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    std::fprintf(stderr, "cannot open lupdate manifest %s\n", PATCHY_LUPDATE_MANIFEST);
    CHECK(false);
  }
  QTextStream in(&file);
  while (!in.atEnd()) {
    const auto line = in.readLine().trimmed();
    if (line.isEmpty()) {
      continue;
    }
    const auto take = [&line](const char* prefix, QString* target) {
      const auto key = QString::fromLatin1(prefix);
      if (!line.startsWith(key)) {
        return false;
      }
      *target = line.mid(key.size());
      return true;
    };
    QString value;
    if (take("lupdate=", &manifest.lupdate) || take("include=", &manifest.include) ||
        take("template=", &manifest.template_path)) {
      continue;
    }
    if (take("options=", &value)) {
      manifest.options = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    } else if (take("languages=", &value)) {
      manifest.languages = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    } else {
      manifest.sources.push_back(line);
    }
  }
  CHECK(!manifest.lupdate.isEmpty());
  CHECK(!manifest.template_path.isEmpty());
  CHECK(!manifest.languages.isEmpty());
  CHECK(!manifest.sources.isEmpty());
  return manifest;
}

void report(const QString& heading, const QStringList& lines) {
  if (lines.isEmpty()) {
    return;
  }
  std::fprintf(stderr, "%s (%lld):\n", qPrintable(heading), static_cast<long long>(lines.size()));
  const auto shown = std::min<qsizetype>(lines.size(), 60);
  for (qsizetype index = 0; index < shown; ++index) {
    std::fprintf(stderr, "  %s\n", qPrintable(lines[index]));
  }
  if (shown < lines.size()) {
    std::fprintf(stderr, "  ... and %lld more\n", static_cast<long long>(lines.size() - shown));
  }
}

// Plural forms Qt expects per shipped language (QTranslator numerus rules).
int expected_plural_forms(const QString& language) {
  if (language == QStringLiteral("ja") || language.startsWith(QStringLiteral("zh"))) {
    return 1;
  }
  return 2;
}

QStringList placeholder_problems(const CatalogMessage& message) {
  QStringList problems;
  static const QRegularExpression argument_pattern(QStringLiteral("%[1-9]"));
  // Qt reads "&&" as a literal ampersand and "& " as no accelerator; any other character
  // after "&" is the accelerator letter, which is often non-ASCII ("&Ö", "&É").
  static const QRegularExpression accelerator_pattern(QStringLiteral("&[^&\\s]"));
  QSet<QString> arguments;
  for (auto it = argument_pattern.globalMatch(message.source); it.hasNext();) {
    arguments.insert(it.next().captured());
  }
  const bool source_has_count = message.source.contains(QStringLiteral("%n"));
  const auto source_ctrl = message.source.count(QStringLiteral("%CTRL%"));
  const auto source_alt = message.source.count(QStringLiteral("%ALT%"));
  const bool source_accelerator = accelerator_pattern.match(message.source).hasMatch();
  const bool source_ellipsis = message.source.endsWith(QStringLiteral("..."));
  const bool source_colon = message.source.endsWith(QLatin1Char(':'));
  bool any_form_has_count = false;
  for (const auto& translation : message.translations) {
    for (const auto& argument : arguments) {
      if (!translation.contains(argument)) {
        problems << QStringLiteral("missing %1").arg(argument);
      }
    }
    any_form_has_count = any_form_has_count || translation.contains(QStringLiteral("%n"));
    if (translation.count(QStringLiteral("%CTRL%")) != source_ctrl) {
      problems << QStringLiteral("%CTRL% token count differs");
    }
    if (translation.count(QStringLiteral("%ALT%")) != source_alt) {
      problems << QStringLiteral("%ALT% token count differs");
    }
    if (source_accelerator && !accelerator_pattern.match(translation).hasMatch()) {
      problems << QStringLiteral("accelerator (&) dropped");
    }
    if (source_ellipsis && !(translation.endsWith(QStringLiteral("...")) || translation.endsWith(QStringLiteral("…")))) {
      problems << QStringLiteral("trailing ellipsis dropped");
    }
    if (source_colon && !(translation.endsWith(QLatin1Char(':')) || translation.endsWith(QStringLiteral("：")))) {
      problems << QStringLiteral("trailing colon dropped");
    }
  }
  if (source_has_count && !any_form_has_count) {
    problems << QStringLiteral("%n missing from every plural form");
  }
  problems.removeDuplicates();
  return problems;
}

void ui_translation_template_is_current() {
  const auto manifest = read_manifest();
  CHECK(QFileInfo::exists(manifest.lupdate));
  QTemporaryDir temp;
  CHECK(temp.isValid());
  const auto list_path = temp.filePath(QStringLiteral("sources.lst"));
  {
    QFile list(list_path);
    CHECK(list.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&list);
    for (const auto& source : manifest.sources) {
      out << source << '\n';
    }
  }
  const auto output = temp.filePath(QStringLiteral("patchy_en.ts"));
  QStringList arguments = manifest.options;
  arguments << QStringLiteral("-I") << manifest.include << (QStringLiteral("@") + list_path)
            << QStringLiteral("-ts") << output;
  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(manifest.lupdate, arguments);
  CHECK(process.waitForStarted(30000));
  CHECK(process.waitForFinished(300000));
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    std::fprintf(stderr, "lupdate failed:\n%s\n", process.readAll().constData());
    CHECK(false);
  }

  const auto fresh = read_catalog(output);
  const auto committed = read_catalog(manifest.template_path);
  CHECK(committed.language == QStringLiteral("en"));
  CHECK(committed.source_language == QStringLiteral("en"));
  CHECK(!fresh.messages.empty());

  QHash<QString, bool> fresh_index;
  for (const auto& message : fresh.messages) {
    fresh_index.insert(message_key(message), message.numerus);
  }
  QHash<QString, bool> committed_index;
  for (const auto& message : committed.messages) {
    committed_index.insert(message_key(message), message.numerus);
  }
  QStringList missing;
  QStringList stale;
  for (const auto& message : fresh.messages) {
    const auto it = committed_index.constFind(message_key(message));
    if (it == committed_index.constEnd()) {
      missing << describe(message);
    } else if (*it != message.numerus) {
      missing << describe(message) + QStringLiteral(" [plural form changed]");
    }
  }
  for (const auto& message : committed.messages) {
    if (!fresh_index.contains(message_key(message))) {
      stale << describe(message);
    }
  }
  report(QStringLiteral("Strings in the code but not in translations/patchy_en.ts (run scripts\\update-translations.ps1)"), missing);
  report(QStringLiteral("Strings in translations/patchy_en.ts that the code no longer uses (run scripts\\update-translations.ps1)"), stale);
  CHECK(missing.isEmpty());
  CHECK(stale.isEmpty());
}

void ui_translation_catalogs_are_complete() {
  const auto manifest = read_manifest();
  const auto template_catalog = read_catalog(manifest.template_path);
  const auto template_dir = QFileInfo(manifest.template_path).dir();
  QHash<QString, const CatalogMessage*> template_index;
  for (const auto& message : template_catalog.messages) {
    template_index.insert(message_key(message), &message);
  }
  const auto runtime_dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations"));

  QStringList failures;
  for (const auto& language : manifest.languages) {
    const auto path = template_dir.filePath(QStringLiteral("patchy_%1.ts").arg(language));
    if (!QFileInfo::exists(path)) {
      failures << QStringLiteral("%1: catalog file missing").arg(language);
      continue;
    }
    const auto catalog = read_catalog(path);
    if (catalog.language != language) {
      failures << QStringLiteral("%1: TS language attribute is '%2'").arg(language, catalog.language);
    }
    if (catalog.source_language != QStringLiteral("en")) {
      failures << QStringLiteral("%1: TS sourcelanguage attribute is '%2'").arg(language, catalog.source_language);
    }
    QSet<QString> seen;
    for (const auto& message : catalog.messages) {
      const auto key = message_key(message);
      seen.insert(key);
      const auto prefix = QStringLiteral("%1: %2: ").arg(language, describe(message));
      if (!template_index.contains(key)) {
        failures << prefix + QStringLiteral("not in the template (stale; run scripts\\update-translations.ps1)");
        continue;
      }
      if (!message.type.isEmpty()) {
        failures << prefix + QStringLiteral("translation type is '%1'").arg(message.type);
        continue;
      }
      if (message.translations.isEmpty()) {
        failures << prefix + QStringLiteral("no translation");
        continue;
      }
      if (message.numerus && message.translations.size() != expected_plural_forms(language)) {
        failures << prefix + QStringLiteral("%1 plural forms, expected %2")
                                .arg(message.translations.size())
                                .arg(expected_plural_forms(language));
      }
      for (const auto& translation : message.translations) {
        if (translation.trimmed().isEmpty()) {
          failures << prefix + QStringLiteral("empty translation");
          break;
        }
      }
      for (const auto& problem : placeholder_problems(message)) {
        failures << prefix + problem;
      }
    }
    for (const auto& message : template_catalog.messages) {
      if (!seen.contains(message_key(message))) {
        failures << QStringLiteral("%1: %2: missing from the catalog (run scripts\\update-translations.ps1)")
                        .arg(language, describe(message));
      }
    }
    for (const auto& file : {QStringLiteral("patchy_%1.qm"), QStringLiteral("qtbase_%1.qm")}) {
      const auto qm = QDir(runtime_dir).filePath(file.arg(language));
      if (!QFileInfo::exists(qm)) {
        failures << QStringLiteral("%1: %2 is not built next to the test binary").arg(language, qm);
      }
    }
  }
  report(QStringLiteral("Translation catalog problems"), failures);
  CHECK(failures.isEmpty());
}

void ui_translation_template_covers_runtime_sources() {
  const auto manifest = read_manifest();
  const auto template_catalog = read_catalog(manifest.template_path);
  QSet<QString> known;
  for (const auto& message : template_catalog.messages) {
    known.insert(message.context + QLatin1Char('\x1f') + message.source);
  }
  QStringList missing;
  const auto check = [&](const QString& context, const QString& source, const QString& where) {
    if (source.isEmpty()) {
      return;
    }
    if (!known.contains(context + QLatin1Char('\x1f') + source)) {
      missing << QStringLiteral("%1: [%2] %3").arg(where, context, source.left(90));
    }
  };

  patchy::ui::MainWindow window;
  show_window(window);
  const auto main_context = QString::fromLatin1(patchy::ui::kMainWindowTranslationContext);
  const auto visit = [&](const QObject* object) {
    auto context = object->property(patchy::ui::kTranslationContextProperty).toString();
    if (context.isEmpty()) {
      context = main_context;
    }
    const auto where = object->objectName().isEmpty() ? QString::fromLatin1(object->metaObject()->className())
                                                      : object->objectName();
    for (const auto* property : {patchy::ui::kTranslationTextProperty, patchy::ui::kTranslationToolTipProperty,
                                 patchy::ui::kTranslationStatusTipProperty}) {
      const auto value = object->property(property);
      if (value.isValid()) {
        check(context, value.toString(), where);
      }
    }
    const auto detail = object->property(patchy::ui::kActionTooltipDetailProperty);
    if (detail.isValid()) {
      check(main_context, detail.toString(), where + QStringLiteral(" tooltip detail"));
    }
  };
  visit(&window);
  const auto children = window.findChildren<QObject*>();
  for (const auto* child : children) {
    visit(child);
  }

  const auto data_context = QString::fromLatin1(patchy::ui::kDataTranslationContext);
  const auto check_data = [&](const char* text, const char* where) {
    check(data_context, QString::fromUtf8(text), QString::fromLatin1(where));
  };
  for (const auto& preset : patchy::builtin_pattern_presets()) {
    check_data(preset.english_name, "pattern preset");
  }
  for (const auto& preset : patchy::photo_pattern_presets()) {
    check_data(preset.english_name, "photo pattern preset");
  }
  for (const auto& preset : patchy::builtin_contour_presets()) {
    check_data(preset.english_name, "contour preset");
  }
  for (const auto& preset : patchy::builtin_style_presets()) {
    check_data(preset.english_name, "style preset");
    check_data(preset.english_folder, "style preset folder");
  }
  for (const auto& preset : patchy::builtin_gradient_presets()) {
    check_data(preset.english_name, "gradient preset");
    check_data(preset.english_folder, "gradient preset folder");
  }
  for (const auto& preset : patchy::builtin_palette_presets()) {
    check_data(preset.english_name, "palette preset");
  }
  for (const auto& shape : patchy::ui::builtin_custom_shapes()) {
    check_data(shape.english_name, "custom shape");
    check_data(shape.english_folder, "custom shape folder");
  }

  report(QStringLiteral("Runtime-translated strings missing from translations/patchy_en.ts"), missing);
  CHECK(missing.isEmpty());
}

}  // namespace

std::vector<patchy::test::TestCase> localization_tests() {
  return {
      {"ui_translation_template_is_current", ui_translation_template_is_current},
      {"ui_translation_catalogs_are_complete", ui_translation_catalogs_are_complete},
      {"ui_translation_template_covers_runtime_sources", ui_translation_template_covers_runtime_sources},
  };
}
