// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QVariantMap>

#include <functional>

namespace TokenConverter
{

/**
 * QML-facing settings model. The same class backs both a project's settings
 * and the global defaults, so the QML SettingsForm is written once.
 *
 * "reference" is the baseline modified-indicators compare against: the
 * current global defaults for a project, factory defaults for the global page.
 */
class SettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY valuesChanged)
    Q_PROPERTY(QStringList modifiedKeys READ modifiedKeys NOTIFY modifiedKeysChanged)
    Q_PROPERTY(bool isGlobal READ isGlobal CONSTANT)
    Q_PROPERTY(bool advancedVisible READ advancedVisible WRITE setAdvancedVisible NOTIFY advancedVisibleChanged)
    Q_PROPERTY(QStringList targetFormats READ targetFormats CONSTANT)
    Q_PROPERTY(QStringList allKeys READ allKeys CONSTANT)
    Q_PROPERTY(int idealWorkerCount READ idealWorkerCount CONSTANT)

public:
    using Saver = std::function<void(const QVariantMap &values, bool advancedVisible)>;

    SettingsController(bool isGlobal, const QVariantMap &values, const QVariantMap &reference, bool advancedVisible, QObject *parent = nullptr);

    QVariantMap values() const;
    QStringList modifiedKeys() const;
    bool isGlobal() const;
    bool advancedVisible() const;
    void setAdvancedVisible(bool visible);
    QStringList targetFormats() const;
    QStringList allKeys() const;
    int idealWorkerCount() const;

    void setSaver(Saver saver);
    void setReference(const QVariantMap &reference);
    QVariantMap reference() const;
    /// Replaces all values (e.g. "apply to all projects").
    void setValues(const QVariantMap &values);

    Q_INVOKABLE QVariant value(const QString &key) const;
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE bool isModified(const QString &key) const;
    /// Resets one key to the reference value.
    Q_INVOKABLE void resetValue(const QString &key);
    /// Resets everything to the reference (project: global defaults; global: factory).
    Q_INVOKABLE void resetAll();

    /// Resulting file name for @p exampleFile under the current rename settings.
    Q_INVOKABLE QString previewFileName(const QString &exampleFile) const;
    /// A plausible example source file name derived from the first rule.
    Q_INVOKABLE QString exampleFileName() const;

Q_SIGNALS:
    void valuesChanged();
    void modifiedKeysChanged();
    void advancedVisibleChanged();

private:
    void commit(const QVariantMap &values);
    void save();

    bool m_isGlobal;
    QVariantMap m_values;
    QVariantMap m_reference;
    QStringList m_modified;
    bool m_advancedVisible;
    Saver m_saver;
};

} // namespace TokenConverter
