// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "iconverter.h"

#include <QReadWriteLock>

#include <memory>
#include <vector>

namespace TokenConverter
{

/// Thread-safe registry of converters. Later-registered converters take
/// precedence, so a specialised converter can override a generic one.
class ConverterRegistry
{
public:
    ConverterRegistry() = default;
    Q_DISABLE_COPY_MOVE(ConverterRegistry)

    /// The application-wide registry, pre-populated with the built-in converters.
    static ConverterRegistry &instance();

    void registerConverter(std::shared_ptr<IConverter> converter);

    std::shared_ptr<IConverter> find(const QString &sourceFormat, const QString &targetFormat) const;
    std::shared_ptr<IConverter> byId(const QString &id) const;
    std::vector<std::shared_ptr<IConverter>> converters() const;

    QStringList allTargetFormats() const;

private:
    mutable QReadWriteLock m_lock;
    std::vector<std::shared_ptr<IConverter>> m_converters;
};

} // namespace TokenConverter
