// SPDX-License-Identifier: GPL-3.0-or-later
#include "converterregistry.h"
#include "imageconverter.h"

#include <algorithm>

namespace TokenConverter
{

ConverterRegistry &ConverterRegistry::instance()
{
    static ConverterRegistry *registry = [] {
        auto *r = new ConverterRegistry;
        r->registerConverter(std::make_shared<ImageConverter>());
        // Future: r->registerConverter(std::make_shared<FfmpegConverter>());
        return r;
    }();
    return *registry;
}

void ConverterRegistry::registerConverter(std::shared_ptr<IConverter> converter)
{
    if (!converter) {
        return;
    }
    QWriteLocker locker(&m_lock);
    m_converters.push_back(std::move(converter));
}

std::shared_ptr<IConverter> ConverterRegistry::find(const QString &sourceFormat, const QString &targetFormat) const
{
    QReadLocker locker(&m_lock);
    for (auto it = m_converters.rbegin(); it != m_converters.rend(); ++it) {
        if ((*it)->canConvert(sourceFormat, targetFormat)) {
            return *it;
        }
    }
    return nullptr;
}

std::shared_ptr<IConverter> ConverterRegistry::byId(const QString &id) const
{
    QReadLocker locker(&m_lock);
    for (const auto &c : m_converters) {
        if (c->id() == id) {
            return c;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<IConverter>> ConverterRegistry::converters() const
{
    QReadLocker locker(&m_lock);
    return m_converters;
}

QStringList ConverterRegistry::allTargetFormats() const
{
    QReadLocker locker(&m_lock);
    QStringList out;
    for (const auto &c : m_converters) {
        for (const auto &f : c->targetFormats()) {
            if (!out.contains(f)) {
                out << f;
            }
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace TokenConverter
