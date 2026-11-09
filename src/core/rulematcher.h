// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "settings.h"

#include <optional>

namespace TokenConverter
{

/// Matches file names against conversion rules (case-insensitive extensions).
class RuleMatcher
{
public:
    explicit RuleMatcher(const QList<ConversionRule> &rules);

    /// Returns the first rule matching the file's extension.
    std::optional<ConversionRule> match(const QString &fileName) const;

    /// Lower-cased extension after the last dot ("" when none, or for dotfiles like ".png").
    static QString extensionOf(const QString &fileName);

private:
    QList<ConversionRule> m_rules;
};

} // namespace TokenConverter
