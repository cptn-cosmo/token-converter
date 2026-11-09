// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "iconverter.h"

#include <QByteArray>

class QImage;

namespace TokenConverter
{

/**
 * Image converter. Decodes anything Qt's image plugins can read; encodes WebP
 * with libwebp directly (quality, lossless, EXIF/ICC via libwebpmux) and other
 * formats via QImageWriter.
 */
class ImageConverter : public IConverter
{
public:
    ImageConverter();

    QString id() const override;
    QString displayName() const override;
    QStringList sourceFormats() const override;
    QStringList targetFormats() const override;
    ConvertResult convert(const QString &input, const QString &output, const QString &targetFormat, const ConvertOptions &options) const override;
    bool verify(const QString &output, const QString &targetFormat, QString *error) const override;

    /// Extracts a raw EXIF block (TIFF header onwards) from JPEG, PNG or WebP data.
    static QByteArray extractExif(const QByteArray &fileData);

private:
    ConvertResult encodeWebP(const QImage &image, const QString &output, const ConvertOptions &options, const QByteArray &exif) const;
    ConvertResult encodeWithQt(const QImage &image, const QString &output, const QString &targetFormat, const ConvertOptions &options) const;

    QStringList m_sources;
    QStringList m_targets;
};

} // namespace TokenConverter
