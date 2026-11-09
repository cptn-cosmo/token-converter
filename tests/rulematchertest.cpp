// SPDX-License-Identifier: GPL-3.0-or-later
#include "rulematcher.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace TokenConverter;

class RuleMatcherTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void extension()
    {
        QCOMPARE(RuleMatcher::extensionOf(u"a.PNG"_s), u"png"_s);
        QCOMPARE(RuleMatcher::extensionOf(u"dir.d/file"_s), QString());
        QCOMPARE(RuleMatcher::extensionOf(u"archive.tar.gz"_s), u"gz"_s);
        QCOMPARE(RuleMatcher::extensionOf(u".png"_s), QString()); // dotfile, no extension
        QCOMPARE(RuleMatcher::extensionOf(u"noext"_s), QString());
    }

    void caseInsensitive()
    {
        const RuleMatcher m({ConversionRule{{u"png"_s}, u"webp"_s}});
        QVERIFY(m.match(u"x.png"_s));
        QVERIFY(m.match(u"x.PNG"_s));
        QVERIFY(m.match(u"x.Png"_s));
        QCOMPARE(m.match(u"x.PNG"_s)->targetFormat, u"webp"_s);
        QVERIFY(!m.match(u"x.jpg"_s));
        QVERIFY(!m.match(u"png"_s));
    }

    void normalizesRuleInput()
    {
        const RuleMatcher m({ConversionRule{{u".JPG"_s, u"*.jpeg"_s}, u".WEBP"_s}});
        QVERIFY(m.match(u"a.jpg"_s));
        QVERIFY(m.match(u"a.JPEG"_s));
        QCOMPARE(m.match(u"a.jpg"_s)->targetFormat, u"webp"_s);
    }

    void firstRuleWins()
    {
        const RuleMatcher m({ConversionRule{{u"png"_s}, u"webp"_s}, ConversionRule{{u"png"_s, u"bmp"_s}, u"jpg"_s}});
        QCOMPARE(m.match(u"a.png"_s)->targetFormat, u"webp"_s);
        QCOMPARE(m.match(u"a.bmp"_s)->targetFormat, u"jpg"_s);
    }

    void ignoresIncompleteRules()
    {
        const RuleMatcher m({ConversionRule{{}, u"webp"_s}, ConversionRule{{u"png"_s}, {}}});
        QVERIFY(!m.match(u"a.png"_s));
    }
};

QTEST_GUILESS_MAIN(RuleMatcherTest)
#include "rulematchertest.moc"
