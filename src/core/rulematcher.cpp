// SPDX-License-Identifier: GPL-3.0-or-later
#include "rulematcher.h"

namespace TokenConverter
{

RuleMatcher::RuleMatcher(const QList<ConversionRule> &rules)
{
    for (auto rule : rules) {
        QStringList normalizedSources;
        for (const auto &ext : std::as_const(rule.sourceExtensions)) {
            normalizedSources << Settings::normalizeExtension(ext);
        }
        rule.sourceExtensions = normalizedSources;
        rule.targetFormat = Settings::normalizeExtension(rule.targetFormat);
        if (!rule.sourceExtensions.isEmpty() && !rule.targetFormat.isEmpty()) {
            m_rules << rule;
        }
    }
}

QString RuleMatcher::extensionOf(const QString &fileName)
{
    const qsizetype slash = fileName.lastIndexOf(u'/');
    const QString base = slash >= 0 ? fileName.mid(slash + 1) : fileName;
    const qsizetype dot = base.lastIndexOf(u'.');
    if (dot <= 0) { // no dot, or hidden file without extension
        return {};
    }
    return base.mid(dot + 1).toLower();
}

std::optional<ConversionRule> RuleMatcher::match(const QString &fileName) const
{
    const QString ext = extensionOf(fileName);
    if (ext.isEmpty()) {
        return std::nullopt;
    }
    for (const auto &rule : m_rules) {
        if (rule.sourceExtensions.contains(ext)) {
            return rule;
        }
    }
    return std::nullopt;
}

} // namespace TokenConverter
