// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingscontroller.h"

#include "converterregistry.h"
#include "filenametemplate.h"
#include "rulematcher.h"
#include "settings.h"

#include <QJSValue>
#include <QThread>

using namespace Qt::StringLiterals;

namespace TokenConverter
{

SettingsController::SettingsController(bool isGlobal, const QVariantMap &values, const QVariantMap &reference, bool advancedVisible, QObject *parent)
    : QObject(parent)
    , m_isGlobal(isGlobal)
    , m_values(Settings::normalized(values))
    , m_reference(Settings::normalized(reference))
    , m_advancedVisible(advancedVisible)
{
    m_modified = Settings::differingKeys(m_values, m_reference);
}

QVariantMap SettingsController::values() const
{
    return m_values;
}

QStringList SettingsController::modifiedKeys() const
{
    return m_modified;
}

bool SettingsController::isGlobal() const
{
    return m_isGlobal;
}

bool SettingsController::advancedVisible() const
{
    return m_advancedVisible;
}

void SettingsController::setAdvancedVisible(bool visible)
{
    if (m_advancedVisible == visible) {
        return;
    }
    m_advancedVisible = visible;
    Q_EMIT advancedVisibleChanged();
    save();
}

QStringList SettingsController::targetFormats() const
{
    return ConverterRegistry::instance().allTargetFormats();
}

QStringList SettingsController::allKeys() const
{
    return Settings::keys();
}

int SettingsController::idealWorkerCount() const
{
    return std::max(1, QThread::idealThreadCount());
}

void SettingsController::setSaver(Saver saver)
{
    m_saver = std::move(saver);
}

void SettingsController::setReference(const QVariantMap &reference)
{
    m_reference = Settings::normalized(reference);
    const QStringList modified = Settings::differingKeys(m_values, m_reference);
    if (modified != m_modified) {
        m_modified = modified;
        Q_EMIT modifiedKeysChanged();
    }
}

QVariantMap SettingsController::reference() const
{
    return m_reference;
}

void SettingsController::setValues(const QVariantMap &values)
{
    commit(Settings::normalized(values));
}

QVariant SettingsController::value(const QString &key) const
{
    return m_values.value(key);
}

void SettingsController::setValue(const QString &key, const QVariant &value)
{
    if (!Settings::keys().contains(key)) {
        qWarning("SettingsController: unknown key %s", qPrintable(key));
        return;
    }
    QVariant v = value;
    if (v.canConvert<QJSValue>()) {
        v = v.value<QJSValue>().toVariant();
    }
    QVariantMap next = m_values;
    next[key] = v;
    commit(Settings::normalized(next, m_values));
}

bool SettingsController::isModified(const QString &key) const
{
    return m_modified.contains(key);
}

void SettingsController::resetValue(const QString &key)
{
    commit(Inheritance::resetKey(m_values, m_reference, key));
}

void SettingsController::resetAll()
{
    commit(Inheritance::resetAll(m_reference));
}

void SettingsController::commit(const QVariantMap &values)
{
    if (Settings::differingKeys(values, m_values).isEmpty()) {
        return;
    }
    m_values = values;
    Q_EMIT valuesChanged();
    const QStringList modified = Settings::differingKeys(m_values, m_reference);
    if (modified != m_modified) {
        m_modified = modified;
        Q_EMIT modifiedKeysChanged();
    }
    save();
}

void SettingsController::save()
{
    if (m_saver) {
        m_saver(m_values, m_advancedVisible);
    }
}

QString SettingsController::exampleFileName() const
{
    const auto rules = Settings::rulesFromVariant(m_values.value(Settings::Keys::Rules));
    for (const auto &rule : rules) {
        if (!rule.sourceExtensions.isEmpty()) {
            return u"holiday-photo."_s + rule.sourceExtensions.first();
        }
    }
    return u"holiday-photo.png"_s;
}

QString SettingsController::previewFileName(const QString &exampleFile) const
{
    const ConversionSettings s = Settings::toTyped(m_values);
    const QString file = exampleFile.isEmpty() ? exampleFileName() : exampleFile;
    const auto rule = RuleMatcher(s.rules).match(file);
    const qsizetype dot = file.lastIndexOf(u'.');

    TemplateContext ctx;
    ctx.name = dot > 0 ? file.left(dot) : file;
    ctx.ext = RuleMatcher::extensionOf(file);
    ctx.dateTime = QDateTime::currentDateTime();
    ctx.counter = 1;
    ctx.parent = u"vacation"_s;
    ctx.width = 1920;
    ctx.height = 1080;
    return FilenameTemplate::buildFileName(s.prefix, s.filenameTemplate, s.postfix, ctx, rule ? rule->targetFormat : ctx.ext);
}

} // namespace TokenConverter
