// SPDX-License-Identifier: GPL-3.0-or-later
#include "imageconverter.h"

#include <KLocalizedString>

#include <QBuffer>
#include <QColorSpace>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>
#include <QtEndian>

#include <webp/encode.h>
#include <webp/decode.h>
#include <webp/mux.h>

#include <memory>

using namespace Qt::StringLiterals;

namespace TokenConverter
{

ImageConverter::ImageConverter()
{
    for (const QByteArray &f : QImageReader::supportedImageFormats()) {
        m_sources << QString::fromLatin1(f).toLower();
    }
    for (const QByteArray &f : QImageWriter::supportedImageFormats()) {
        m_targets << QString::fromLatin1(f).toLower();
    }
    if (!m_targets.contains(u"webp"_s)) {
        m_targets << u"webp"_s;
    }
    // Common aliases
    if (m_sources.contains(u"jpeg"_s) && !m_sources.contains(u"jpg"_s)) {
        m_sources << u"jpg"_s;
    }
    if (m_targets.contains(u"jpeg"_s) && !m_targets.contains(u"jpg"_s)) {
        m_targets << u"jpg"_s;
    }
    // Formats that make no sense as conversion targets
    m_targets.removeAll(u"cur"_s);
    m_targets.removeAll(u"ico"_s);
}

QString ImageConverter::id() const
{
    return u"image"_s;
}

QString ImageConverter::displayName() const
{
    return i18nc("@info converter name", "Images (libwebp / Qt image plugins)");
}

QStringList ImageConverter::sourceFormats() const
{
    return m_sources;
}

QStringList ImageConverter::targetFormats() const
{
    return m_targets;
}

namespace
{
struct ProgressHookData {
    const ConvertOptions *options;
};

int progressHook(int /*percent*/, const WebPPicture *picture)
{
    const auto *data = static_cast<const ProgressHookData *>(picture->user_data);
    return data && data->options->cancelled() ? 0 : 1;
}

QString webpErrorString(WebPEncodingError err)
{
    switch (err) {
    case VP8_ENC_ERROR_OUT_OF_MEMORY:
    case VP8_ENC_ERROR_BITSTREAM_OUT_OF_MEMORY:
        return i18n("Out of memory while encoding");
    case VP8_ENC_ERROR_BAD_DIMENSION:
        return i18n("Image dimensions are not supported by WebP (max 16383×16383)");
    case VP8_ENC_ERROR_PARTITION0_OVERFLOW:
    case VP8_ENC_ERROR_PARTITION_OVERFLOW:
        return i18n("Encoder partition overflow");
    case VP8_ENC_ERROR_FILE_TOO_BIG:
        return i18n("Encoded file would be too large");
    case VP8_ENC_ERROR_USER_ABORT:
        return i18n("Cancelled");
    default:
        return i18n("WebP encoding failed (code %1)", int(err));
    }
}

bool writeFile(const QString &path, const QByteArray &data, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = i18n("Cannot create output file: %1", f.errorString());
        return false;
    }
    if (f.write(data) != data.size() || !f.flush()) {
        *error = i18n("Cannot write output file: %1", f.errorString());
        f.close();
        return false;
    }
    f.close();
    if (f.error() != QFileDevice::NoError) {
        *error = i18n("Cannot write output file: %1", f.errorString());
        return false;
    }
    return true;
}

QByteArray exifFromJpeg(const QByteArray &d)
{
    if (d.size() < 4 || uchar(d[0]) != 0xFF || uchar(d[1]) != 0xD8) {
        return {};
    }
    qsizetype pos = 2;
    while (pos + 4 <= d.size()) {
        if (uchar(d[pos]) != 0xFF) {
            return {};
        }
        const uchar marker = uchar(d[pos + 1]);
        if (marker == 0xDA || marker == 0xD9) { // start of scan / end of image
            return {};
        }
        const int len = qFromBigEndian<quint16>(d.constData() + pos + 2);
        if (len < 2 || pos + 2 + len > d.size()) {
            return {};
        }
        if (marker == 0xE1 && len >= 8 && d.mid(pos + 4, 6) == QByteArrayLiteral("Exif\0\0")) {
            return d.mid(pos + 10, len - 8);
        }
        pos += 2 + len;
    }
    return {};
}

QByteArray exifFromPng(const QByteArray &d)
{
    static const QByteArray sig = QByteArrayLiteral("\x89PNG\r\n\x1a\n");
    if (!d.startsWith(sig)) {
        return {};
    }
    qsizetype pos = sig.size();
    while (pos + 12 <= d.size()) {
        const quint32 len = qFromBigEndian<quint32>(d.constData() + pos);
        const QByteArray type = d.mid(pos + 4, 4);
        if (pos + 12 + qsizetype(len) > d.size()) {
            return {};
        }
        if (type == "eXIf") {
            return d.mid(pos + 8, len);
        }
        if (type == "IEND" || type == "IDAT") {
            // eXIf must precede IDAT per spec
            return {};
        }
        pos += 12 + len;
    }
    return {};
}

QByteArray exifFromWebp(const QByteArray &d)
{
    WebPData data{reinterpret_cast<const uint8_t *>(d.constData()), size_t(d.size())};
    std::unique_ptr<WebPMux, decltype(&WebPMuxDelete)> mux(WebPMuxCreate(&data, 0), &WebPMuxDelete);
    if (!mux) {
        return {};
    }
    WebPData chunk;
    if (WebPMuxGetChunk(mux.get(), "EXIF", &chunk) == WEBP_MUX_OK) {
        return QByteArray(reinterpret_cast<const char *>(chunk.bytes), qsizetype(chunk.size));
    }
    return {};
}
} // namespace

QByteArray ImageConverter::extractExif(const QByteArray &fileData)
{
    QByteArray exif = exifFromJpeg(fileData);
    if (exif.isEmpty()) {
        exif = exifFromPng(fileData);
    }
    if (exif.isEmpty() && fileData.startsWith("RIFF") && fileData.mid(8, 4) == "WEBP") {
        exif = exifFromWebp(fileData);
    }
    return exif;
}

ConvertResult ImageConverter::convert(const QString &input, const QString &output, const QString &targetFormat, const ConvertOptions &options) const
{
    if (options.cancelled()) {
        return ConvertResult::aborted();
    }

    QFile in(input);
    if (!in.open(QIODevice::ReadOnly)) {
        return ConvertResult::failure(i18n("Cannot read file: %1", in.errorString()));
    }
    const QByteArray raw = in.readAll();
    if (in.error() != QFileDevice::NoError) {
        return ConvertResult::failure(i18n("Cannot read file: %1", in.errorString()));
    }
    in.close();

    QImage image;
    {
        QBuffer buffer;
        buffer.setData(raw);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        reader.setAutoTransform(!options.preserveMetadata); // keep orientation tag with EXIF, otherwise bake it in
        reader.setAllocationLimit(1024); // MiB
        if (!reader.read(&image)) {
            return ConvertResult::failure(i18n("Cannot decode image (corrupt or unsupported): %1", reader.errorString()));
        }
    }
    if (image.isNull()) {
        return ConvertResult::failure(i18n("Cannot decode image (corrupt or unsupported)"));
    }
    if (options.cancelled()) {
        return ConvertResult::aborted();
    }

    const QString fmt = targetFormat.toLower();
    if (fmt == u"webp"_s) {
        const QByteArray exif = options.preserveMetadata ? extractExif(raw) : QByteArray();
        return encodeWebP(image, output, options, exif);
    }
    return encodeWithQt(image, output, fmt, options);
}

ConvertResult ImageConverter::encodeWebP(const QImage &source, const QString &output, const ConvertOptions &options, const QByteArray &exif) const
{
    if (source.width() > WEBP_MAX_DIMENSION || source.height() > WEBP_MAX_DIMENSION) {
        return ConvertResult::failure(webpErrorString(VP8_ENC_ERROR_BAD_DIMENSION));
    }

    QImage image = source;
    if (!options.preserveMetadata && image.colorSpace().isValid() && image.colorSpace() != QColorSpace::SRgb) {
        // Without the ICC profile, colours are only right if we convert to sRGB.
        image.convertToColorSpace(QColorSpace::SRgb);
    }
    const bool hasAlpha = image.hasAlphaChannel();
    image = image.convertToFormat(hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888);

    WebPConfig config;
    if (!WebPConfigInit(&config)) {
        return ConvertResult::failure(i18n("libwebp version mismatch"));
    }
    if (options.lossless) {
        WebPConfigLosslessPreset(&config, 6);
        config.exact = 1; // keep RGB under transparent pixels for true losslessness
    } else {
        config.quality = float(std::clamp(options.quality, 0, 100));
        config.method = 4;
        config.alpha_quality = 100;
    }
    config.thread_level = 0; // we already parallelise across files
    if (!WebPValidateConfig(&config)) {
        return ConvertResult::failure(i18n("Invalid WebP encoder configuration"));
    }

    WebPPicture picture;
    if (!WebPPictureInit(&picture)) {
        return ConvertResult::failure(i18n("libwebp version mismatch"));
    }
    picture.use_argb = options.lossless ? 1 : 0;
    picture.width = image.width();
    picture.height = image.height();
    const int ok = hasAlpha ? WebPPictureImportRGBA(&picture, image.constBits(), int(image.bytesPerLine()))
                            : WebPPictureImportRGB(&picture, image.constBits(), int(image.bytesPerLine()));
    if (!ok) {
        WebPPictureFree(&picture);
        return ConvertResult::failure(i18n("Out of memory while preparing image"));
    }

    WebPMemoryWriter writer;
    WebPMemoryWriterInit(&writer);
    picture.writer = WebPMemoryWrite;
    picture.custom_ptr = &writer;
    ProgressHookData hookData{&options};
    picture.user_data = &hookData;
    picture.progress_hook = progressHook;

    const int encoded = WebPEncode(&config, &picture);
    const WebPEncodingError encError = picture.error_code;
    WebPPictureFree(&picture);
    std::unique_ptr<WebPMemoryWriter, decltype(&WebPMemoryWriterClear)> writerGuard(&writer, &WebPMemoryWriterClear);

    if (!encoded) {
        if (encError == VP8_ENC_ERROR_USER_ABORT || options.cancelled()) {
            return ConvertResult::aborted();
        }
        return ConvertResult::failure(webpErrorString(encError));
    }

    QByteArray result(reinterpret_cast<const char *>(writer.mem), qsizetype(writer.size));

    const QByteArray icc = options.preserveMetadata && image.colorSpace().isValid() && image.colorSpace() != QColorSpace::SRgb ? image.colorSpace().iccProfile()
                                                                                                                                 : QByteArray();
    if (!exif.isEmpty() || !icc.isEmpty()) {
        WebPData bitstream{writer.mem, writer.size};
        std::unique_ptr<WebPMux, decltype(&WebPMuxDelete)> mux(WebPMuxCreate(&bitstream, 1), &WebPMuxDelete);
        if (mux) {
            if (!icc.isEmpty()) {
                WebPData d{reinterpret_cast<const uint8_t *>(icc.constData()), size_t(icc.size())};
                WebPMuxSetChunk(mux.get(), "ICCP", &d, 1);
            }
            if (!exif.isEmpty()) {
                WebPData d{reinterpret_cast<const uint8_t *>(exif.constData()), size_t(exif.size())};
                WebPMuxSetChunk(mux.get(), "EXIF", &d, 1);
            }
            WebPData assembled;
            WebPDataInit(&assembled);
            if (WebPMuxAssemble(mux.get(), &assembled) == WEBP_MUX_OK) {
                result = QByteArray(reinterpret_cast<const char *>(assembled.bytes), qsizetype(assembled.size));
            }
            WebPDataClear(&assembled);
        }
    }

    if (options.cancelled()) {
        return ConvertResult::aborted();
    }
    QString error;
    if (!writeFile(output, result, &error)) {
        return ConvertResult::failure(error);
    }
    return ConvertResult::success();
}

ConvertResult ImageConverter::encodeWithQt(const QImage &source, const QString &output, const QString &targetFormat, const ConvertOptions &options) const
{
    QImage image = source;
    if (!options.preserveMetadata && image.colorSpace().isValid() && image.colorSpace() != QColorSpace::SRgb) {
        image.convertToColorSpace(QColorSpace::SRgb);
    }
    if (!options.preserveMetadata) {
        image.setColorSpace(QColorSpace());
    }

    QFile file(output);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return ConvertResult::failure(i18n("Cannot create output file: %1", file.errorString()));
    }
    QImageWriter writer(&file, (targetFormat == u"jpg"_s ? u"jpeg"_s : targetFormat).toLatin1());
    if (options.lossless) {
        writer.setQuality(100);
        writer.setCompression(9); // png: max compression; ignored by others
    } else {
        writer.setQuality(options.quality);
    }
    writer.setOptimizedWrite(true);
    if (!writer.write(image)) {
        const QString err = file.error() != QFileDevice::NoError ? file.errorString() : writer.errorString();
        file.close();
        return ConvertResult::failure(i18n("Encoding failed: %1", err));
    }
    if (!file.flush()) {
        return ConvertResult::failure(i18n("Cannot write output file: %1", file.errorString()));
    }
    file.close();
    return options.cancelled() ? ConvertResult::aborted() : ConvertResult::success();
}

bool ImageConverter::verify(const QString &output, const QString &targetFormat, QString *error) const
{
    QFile f(output);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = i18n("Cannot reopen converted file: %1", f.errorString());
        return false;
    }
    if (f.size() == 0) {
        *error = i18n("Converted file is empty");
        return false;
    }
    if (targetFormat.toLower() == u"webp"_s) {
        // Validate the full bitstream header with libwebp itself.
        const QByteArray head = f.read(64 * 1024);
        WebPBitstreamFeatures features;
        if (WebPGetFeatures(reinterpret_cast<const uint8_t *>(head.constData()), size_t(head.size()), &features) != VP8_STATUS_OK) {
            *error = i18n("Converted file is not a valid WebP image");
            return false;
        }
        return true;
    }
    f.close();
    QImageReader reader(output);
    if (!reader.canRead() || !reader.size().isValid()) {
        *error = i18n("Converted file cannot be read back: %1", reader.errorString());
        return false;
    }
    return true;
}

} // namespace TokenConverter
