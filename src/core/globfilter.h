// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QRegularExpression>
#include <QStringList>

namespace TokenConverter
{

/**
 * Include/exclude filtering for the scanner.
 *
 * Pattern syntax (case-insensitive):
 *   *      any run of characters except '/'
 *   **     any run of characters including '/'
 *   ?      one character except '/'
 *   [abc]  character class ([!abc] negates)
 *
 * A pattern without '/' is matched against the file name only; a pattern
 * containing '/' is matched against the path relative to the project root.
 *
 * Excluded folders are matched against each directory's name (when the entry
 * has no '/') or its relative path (when it has one).
 */
class GlobFilter
{
public:
    GlobFilter(const QStringList &includePatterns, const QStringList &excludePatterns, const QStringList &excludeFolders);

    /// @p relativePath uses '/' separators and is relative to the project root.
    bool acceptsFile(const QString &relativePath) const;
    bool acceptsDirectory(const QString &relativePath) const;

    static QRegularExpression globToRegex(const QString &glob);
    static bool matches(const QString &glob, const QString &relativePath);

private:
    struct Pattern {
        QRegularExpression regex;
        bool pathPattern;
    };
    static QList<Pattern> compile(const QStringList &globs);
    static bool anyMatches(const QList<Pattern> &patterns, const QString &relativePath);

    QList<Pattern> m_include;
    QList<Pattern> m_exclude;
    QList<Pattern> m_excludeFolders;
};

} // namespace TokenConverter
