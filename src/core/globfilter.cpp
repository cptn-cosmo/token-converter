// SPDX-License-Identifier: GPL-3.0-or-later
#include "globfilter.h"

using namespace Qt::StringLiterals;

namespace TokenConverter
{

GlobFilter::GlobFilter(const QStringList &includePatterns, const QStringList &excludePatterns, const QStringList &excludeFolders)
    : m_include(compile(includePatterns))
    , m_exclude(compile(excludePatterns))
    , m_excludeFolders(compile(excludeFolders))
{
}

QRegularExpression GlobFilter::globToRegex(const QString &glob)
{
    QString rx;
    rx.reserve(glob.size() * 2 + 4);
    rx += u'^';
    for (qsizetype i = 0; i < glob.size(); ++i) {
        const QChar c = glob.at(i);
        if (c == u'*') {
            if (i + 1 < glob.size() && glob.at(i + 1) == u'*') {
                ++i;
                // "**/" also matches zero directories
                if (i + 1 < glob.size() && glob.at(i + 1) == u'/') {
                    ++i;
                    rx += u"(?:.*/)?"_s;
                } else {
                    rx += u".*"_s;
                }
            } else {
                rx += u"[^/]*"_s;
            }
        } else if (c == u'?') {
            rx += u"[^/]"_s;
        } else if (c == u'[') {
            const qsizetype close = glob.indexOf(u']', i + 1);
            if (close < 0) {
                rx += u"\\["_s;
                continue;
            }
            QString cls = glob.mid(i + 1, close - i - 1);
            if (cls.startsWith(u'!')) {
                cls[0] = u'^';
            }
            cls.replace(u"\\"_s, u"\\\\"_s);
            rx += u'[' + cls + u']';
            i = close;
        } else {
            rx += QRegularExpression::escape(QString(c));
        }
    }
    rx += u'$';
    return QRegularExpression(rx, QRegularExpression::CaseInsensitiveOption);
}

QList<GlobFilter::Pattern> GlobFilter::compile(const QStringList &globs)
{
    QList<Pattern> out;
    for (QString g : globs) {
        g = g.trimmed();
        while (g.endsWith(u'/') && g.size() > 1) {
            g.chop(1);
        }
        if (g.startsWith(u"./"_s)) {
            g.remove(0, 2);
        }
        if (g.isEmpty()) {
            continue;
        }
        const bool pathPattern = g.contains(u'/');
        if (g.startsWith(u'/')) {
            g.remove(0, 1);
        }
        out << Pattern{globToRegex(g), pathPattern};
    }
    return out;
}

bool GlobFilter::anyMatches(const QList<Pattern> &patterns, const QString &relativePath)
{
    const qsizetype slash = relativePath.lastIndexOf(u'/');
    const QString name = slash >= 0 ? relativePath.mid(slash + 1) : relativePath;
    for (const auto &p : patterns) {
        if (p.regex.match(p.pathPattern ? relativePath : name).hasMatch()) {
            return true;
        }
    }
    return false;
}

bool GlobFilter::matches(const QString &glob, const QString &relativePath)
{
    return anyMatches(compile({glob}), relativePath);
}

bool GlobFilter::acceptsFile(const QString &relativePath) const
{
    // Files inside an excluded folder are never accepted, even if the scanner
    // was asked to start inside it.
    const qsizetype slash = relativePath.lastIndexOf(u'/');
    if (slash > 0) {
        const QStringList parts = relativePath.left(slash).split(u'/');
        QString prefix;
        for (const auto &part : parts) {
            prefix = prefix.isEmpty() ? part : prefix + u'/' + part;
            if (!acceptsDirectory(prefix)) {
                return false;
            }
        }
    }
    if (!m_include.isEmpty() && !anyMatches(m_include, relativePath)) {
        return false;
    }
    return !anyMatches(m_exclude, relativePath);
}

bool GlobFilter::acceptsDirectory(const QString &relativePath) const
{
    return !anyMatches(m_excludeFolders, relativePath);
}

} // namespace TokenConverter
