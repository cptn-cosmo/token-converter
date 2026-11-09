// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QString>

namespace TokenConverter
{

/// Values available to a filename template.
struct TemplateContext {
    QString name; ///< base name without extension
    QString ext; ///< original extension without dot
    QDateTime dateTime; ///< file modification time
    int counter = 1; ///< 1-based position in the batch
    QString parent; ///< name of the containing folder
    int width = -1; ///< image width (-1 if unknown)
    int height = -1; ///< image height (-1 if unknown)
};

/**
 * Expands templates such as "{parent}_{name}_{width}x{height}".
 *
 * Supported placeholders: {name} {ext} {date} (yyyy-MM-dd) {time} (HH-mm-ss)
 * {counter} {parent} {width} {height}. {counter:N} zero-pads to N digits.
 * Unknown placeholders are left untouched. "{{" and "}}" produce literal braces.
 */
namespace FilenameTemplate
{
QString expand(const QString &tmpl, const TemplateContext &ctx);

/// prefix + expand(template or "{name}") + postfix + "." + targetExtension,
/// with path separators and other unsafe characters sanitised.
QString buildFileName(const QString &prefix, const QString &tmpl, const QString &postfix, const TemplateContext &ctx, const QString &targetExtension);

bool usesImageSize(const QString &tmpl);

QString sanitize(const QString &fileName);
}

} // namespace TokenConverter
