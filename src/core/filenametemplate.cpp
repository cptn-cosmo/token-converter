// SPDX-License-Identifier: GPL-3.0-or-later
#include "filenametemplate.h"

#include <algorithm>

using namespace Qt::StringLiterals;

namespace TokenConverter::FilenameTemplate
{

static QString valueFor(const QString &placeholder, const TemplateContext &ctx, bool *known)
{
    *known = true;
    QString key = placeholder;
    QString arg;
    const qsizetype colon = placeholder.indexOf(u':');
    if (colon >= 0) {
        key = placeholder.left(colon);
        arg = placeholder.mid(colon + 1);
    }
    if (key == u"name"_s) {
        return ctx.name;
    }
    if (key == u"ext"_s) {
        return ctx.ext;
    }
    if (key == u"date"_s) {
        return ctx.dateTime.isValid() ? ctx.dateTime.toString(u"yyyy-MM-dd"_s) : QString();
    }
    if (key == u"time"_s) {
        return ctx.dateTime.isValid() ? ctx.dateTime.toString(u"HH-mm-ss"_s) : QString();
    }
    if (key == u"counter"_s) {
        bool ok = false;
        const int width = arg.toInt(&ok);
        return QStringLiteral("%1").arg(ctx.counter, ok ? std::clamp(width, 0, 12) : 0, 10, u'0');
    }
    if (key == u"parent"_s) {
        return ctx.parent;
    }
    if (key == u"width"_s) {
        return ctx.width >= 0 ? QString::number(ctx.width) : QString();
    }
    if (key == u"height"_s) {
        return ctx.height >= 0 ? QString::number(ctx.height) : QString();
    }
    *known = false;
    return {};
}

QString expand(const QString &tmpl, const TemplateContext &ctx)
{
    QString out;
    out.reserve(tmpl.size() + 16);
    for (qsizetype i = 0; i < tmpl.size(); ++i) {
        const QChar c = tmpl.at(i);
        if (c == u'{' && i + 1 < tmpl.size() && tmpl.at(i + 1) == u'{') {
            out += u'{';
            ++i;
            continue;
        }
        if (c == u'}' && i + 1 < tmpl.size() && tmpl.at(i + 1) == u'}') {
            out += u'}';
            ++i;
            continue;
        }
        if (c == u'{') {
            const qsizetype close = tmpl.indexOf(u'}', i + 1);
            if (close > i) {
                const QString placeholder = tmpl.mid(i + 1, close - i - 1);
                bool known = false;
                const QString value = valueFor(placeholder, ctx, &known);
                if (known) {
                    out += value;
                    i = close;
                    continue;
                }
            }
        }
        out += c;
    }
    return out;
}

QString sanitize(const QString &fileName)
{
    QString out = fileName;
    for (auto &c : out) {
        if (c == u'/' || c == u'\\' || c == QChar(0) || c.category() == QChar::Other_Control) {
            c = u'_';
        }
    }
    out = out.trimmed();
    if (out == u"."_s || out == u".."_s) {
        out = u"_"_s;
    }
    return out;
}

QString buildFileName(const QString &prefix, const QString &tmpl, const QString &postfix, const TemplateContext &ctx, const QString &targetExtension)
{
    QString base = expand(tmpl.trimmed().isEmpty() ? u"{name}"_s : tmpl, ctx);
    base = sanitize(prefix + base + postfix);
    if (base.isEmpty()) {
        base = sanitize(ctx.name.isEmpty() ? u"file"_s : ctx.name);
    }
    return targetExtension.isEmpty() ? base : base + u'.' + targetExtension;
}

bool usesImageSize(const QString &tmpl)
{
    return tmpl.contains(u"{width}"_s) || tmpl.contains(u"{height}"_s);
}

} // namespace TokenConverter::FilenameTemplate
