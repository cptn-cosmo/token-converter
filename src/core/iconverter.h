// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <atomic>

namespace TokenConverter
{

struct ConvertOptions {
    bool lossless = false;
    int quality = 80; ///< 0-100, ignored when lossless
    bool preserveMetadata = true;
    /// Set by the job runner on cancel; converters should poll it and abort.
    const std::atomic_bool *cancelRequested = nullptr;

    bool cancelled() const
    {
        return cancelRequested && cancelRequested->load(std::memory_order_relaxed);
    }
};

struct ConvertResult {
    bool ok = false;
    bool cancelled = false;
    QString error;

    static ConvertResult success()
    {
        return {true, false, {}};
    }
    static ConvertResult failure(const QString &error)
    {
        return {false, false, error};
    }
    static ConvertResult aborted()
    {
        return {false, true, {}};
    }
};

/**
 * A pluggable converter. Implementations must be thread-safe: convert() is
 * called concurrently from worker threads. A converter never touches the
 * original file; it only writes @p output (a temp file chosen by SafeWriter).
 */
class IConverter
{
public:
    virtual ~IConverter() = default;

    /// Stable identifier, e.g. "image".
    virtual QString id() const = 0;
    /// Human readable name for the UI.
    virtual QString displayName() const = 0;

    /// Lower-case extensions without dot.
    virtual QStringList sourceFormats() const = 0;
    virtual QStringList targetFormats() const = 0;

    virtual bool canConvert(const QString &sourceFormat, const QString &targetFormat) const
    {
        return sourceFormats().contains(sourceFormat.toLower()) && targetFormats().contains(targetFormat.toLower());
    }

    virtual ConvertResult convert(const QString &input, const QString &output, const QString &targetFormat, const ConvertOptions &options) const = 0;

    /// Sanity check of a freshly written output file before it replaces anything.
    virtual bool verify(const QString &output, const QString &targetFormat, QString *error) const = 0;
};

} // namespace TokenConverter
