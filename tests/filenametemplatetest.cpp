// SPDX-License-Identifier: GPL-3.0-or-later
#include "filenametemplate.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace TokenConverter;

class FilenameTemplateTest : public QObject
{
    Q_OBJECT

    TemplateContext ctx() const
    {
        TemplateContext c;
        c.name = u"photo"_s;
        c.ext = u"png"_s;
        c.dateTime = QDateTime(QDate(2026, 3, 14), QTime(9, 26, 53));
        c.counter = 7;
        c.parent = u"assets"_s;
        c.width = 640;
        c.height = 480;
        return c;
    }

private Q_SLOTS:
    void expand_data()
    {
        QTest::addColumn<QString>("tmpl");
        QTest::addColumn<QString>("expected");
        QTest::newRow("name") << u"{name}"_s << u"photo"_s;
        QTest::newRow("ext") << u"{name}_{ext}"_s << u"photo_png"_s;
        QTest::newRow("date") << u"{date}"_s << u"2026-03-14"_s;
        QTest::newRow("time") << u"{time}"_s << u"09-26-53"_s;
        QTest::newRow("counter") << u"{counter}"_s << u"7"_s;
        QTest::newRow("counter padded") << u"{counter:3}"_s << u"007"_s;
        QTest::newRow("parent") << u"{parent}-{name}"_s << u"assets-photo"_s;
        QTest::newRow("size") << u"{name}@{width}x{height}"_s << u"photo@640x480"_s;
        QTest::newRow("unknown kept") << u"{name}{nope}"_s << u"photo{nope}"_s;
        QTest::newRow("escaped braces") << u"{{name}}"_s << u"{name}"_s;
        QTest::newRow("unterminated") << u"{name"_s << u"{name"_s;
        QTest::newRow("literal only") << u"static"_s << u"static"_s;
        QTest::newRow("empty") << QString() << QString();
    }

    void expand()
    {
        QFETCH(QString, tmpl);
        QFETCH(QString, expected);
        QCOMPARE(FilenameTemplate::expand(tmpl, ctx()), expected);
    }

    void unknownSize()
    {
        TemplateContext c = ctx();
        c.width = -1;
        c.height = -1;
        QCOMPARE(FilenameTemplate::expand(u"{name}_{width}x{height}"_s, c), u"photo_x"_s);
    }

    void buildFileName()
    {
        QCOMPARE(FilenameTemplate::buildFileName({}, {}, {}, ctx(), u"webp"_s), u"photo.webp"_s);
        QCOMPARE(FilenameTemplate::buildFileName(u"pre_"_s, {}, u"_post"_s, ctx(), u"webp"_s), u"pre_photo_post.webp"_s);
        QCOMPARE(FilenameTemplate::buildFileName(u"x-"_s, u"{parent}_{counter:2}"_s, {}, ctx(), u"webp"_s), u"x-assets_07.webp"_s);
        // whitespace-only template falls back to {name}
        QCOMPARE(FilenameTemplate::buildFileName({}, u"   "_s, {}, ctx(), u"webp"_s), u"photo.webp"_s);
    }

    void sanitizesSeparators()
    {
        TemplateContext c = ctx();
        c.parent = u"a/b"_s;
        QCOMPARE(FilenameTemplate::buildFileName({}, u"{parent}/{name}"_s, {}, c, u"webp"_s), u"a_b_photo.webp"_s);
        QCOMPARE(FilenameTemplate::sanitize(u".."_s), u"_"_s);
    }

    void emptyResultFallsBack()
    {
        TemplateContext c = ctx();
        QCOMPARE(FilenameTemplate::buildFileName({}, u"{width}"_s, {}, TemplateContext{c.name, c.ext, {}, 1, {}, -1, -1}, u"webp"_s), u"photo.webp"_s);
    }

    void usesImageSize()
    {
        QVERIFY(FilenameTemplate::usesImageSize(u"{width}"_s));
        QVERIFY(FilenameTemplate::usesImageSize(u"a{height}"_s));
        QVERIFY(!FilenameTemplate::usesImageSize(u"{name}"_s));
    }
};

QTEST_GUILESS_MAIN(FilenameTemplateTest)
#include "filenametemplatetest.moc"
