// SPDX-License-Identifier: GPL-3.0-or-later
#include "globfilter.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace TokenConverter;

class GlobFilterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void matches_data()
    {
        QTest::addColumn<QString>("glob");
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("expected");

        QTest::newRow("name star") << u"*.png"_s << u"a/b/c.png"_s << true;
        QTest::newRow("name star case") << u"*.png"_s << u"C.PNG"_s << true;
        QTest::newRow("name no match") << u"*.png"_s << u"c.jpg"_s << false;
        QTest::newRow("question") << u"img?.png"_s << u"img1.png"_s << true;
        QTest::newRow("question too many") << u"img?.png"_s << u"img12.png"_s << false;
        QTest::newRow("class") << u"img[12].png"_s << u"img2.png"_s << true;
        QTest::newRow("negated class") << u"img[!12].png"_s << u"img2.png"_s << false;
        QTest::newRow("path star stays in dir") << u"assets/*.png"_s << u"assets/sub/a.png"_s << false;
        QTest::newRow("path star") << u"assets/*.png"_s << u"assets/a.png"_s << true;
        QTest::newRow("doublestar") << u"assets/**"_s << u"assets/sub/deep/a.png"_s << true;
        QTest::newRow("doublestar slash zero dirs") << u"assets/**/a.png"_s << u"assets/a.png"_s << true;
        QTest::newRow("doublestar slash many dirs") << u"assets/**/a.png"_s << u"assets/x/y/a.png"_s << true;
        QTest::newRow("leading slash anchors") << u"/top.png"_s << u"top.png"_s << true;
        QTest::newRow("regex chars escaped") << u"a+b(1).png"_s << u"a+b(1).png"_s << true;
        QTest::newRow("dot is literal") << u"a.png"_s << u"aXpng"_s << false;
    }

    void matches()
    {
        QFETCH(QString, glob);
        QFETCH(QString, path);
        QFETCH(bool, expected);
        QCOMPARE(GlobFilter::matches(glob, path), expected);
    }

    void includeExclude()
    {
        const GlobFilter f({u"*.png"_s, u"icons/**"_s}, {u"*_thumb.*"_s}, {});
        QVERIFY(f.acceptsFile(u"a.png"_s));
        QVERIFY(f.acceptsFile(u"icons/x/a.svg"_s));
        QVERIFY(!f.acceptsFile(u"a.jpg"_s));
        QVERIFY(!f.acceptsFile(u"a_thumb.png"_s));
    }

    void emptyIncludeAcceptsAll()
    {
        const GlobFilter f({}, {}, {});
        QVERIFY(f.acceptsFile(u"x/y/z.anything"_s));
    }

    void folderExclusions()
    {
        const GlobFilter f({}, {}, {u".git"_s, u"node_modules"_s, u"build/out"_s, u"cache-*"_s});
        QVERIFY(!f.acceptsDirectory(u".git"_s));
        QVERIFY(!f.acceptsDirectory(u"sub/node_modules"_s));
        QVERIFY(!f.acceptsDirectory(u"build/out"_s));
        QVERIFY(f.acceptsDirectory(u"other/build/out"_s)); // path patterns are anchored at the root
        QVERIFY(!f.acceptsDirectory(u"cache-2026"_s));
        QVERIFY(f.acceptsDirectory(u"src"_s));
        // files inside excluded folders are rejected too
        QVERIFY(!f.acceptsFile(u"sub/node_modules/pkg/a.png"_s));
        QVERIFY(f.acceptsFile(u"src/a.png"_s));
    }

    void trailingSlashTolerated()
    {
        const GlobFilter f({}, {}, {u"dist/"_s});
        QVERIFY(!f.acceptsDirectory(u"dist"_s));
    }
};

QTEST_GUILESS_MAIN(GlobFilterTest)
#include "globfiltertest.moc"
