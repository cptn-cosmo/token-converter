// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.tokenconverter

import "SettingLabels.js" as Labels

/// Lets the user push selected global defaults to every existing project.
/// Nothing is selected by default; nothing happens without confirmation.
Kirigami.Dialog {
    id: dialog

    property var selected: ({})
    readonly property int selectedCount: Object.keys(selected).filter(k => selected[k]).length

    title: i18nc("@title:window", "Apply to All Projects")
    preferredWidth: Kirigami.Units.gridUnit * 26
    padding: Kirigami.Units.largeSpacing

    onOpened: selected = ({})

    standardButtons: Kirigami.Dialog.Cancel
    customFooterActions: [
        Kirigami.Action {
            icon.name: "dialog-ok-apply"
            text: i18ncp("@action:button", "Apply %1 Setting", "Apply %1 Settings", dialog.selectedCount)
            enabled: dialog.selectedCount > 0
            onTriggered: confirm.open()
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18np("Choose which global defaults to copy into the existing project. Other settings stay as they are.",
                        "Choose which global defaults to copy into all %1 existing projects. Other settings stay as they are.",
                        App.projects.count)
        }

        Repeater {
            model: App.globalSettings.allKeys
            delegate: QQC2.CheckBox {
                required property string modelData
                text: Labels.label(modelData)
                checked: dialog.selected[modelData] === true
                onToggled: {
                    const next = Object.assign({}, dialog.selected);
                    next[modelData] = checked;
                    dialog.selected = next;
                }
            }
        }
    }

    Kirigami.PromptDialog {
        id: confirm
        title: i18nc("@title:window", "Overwrite Project Settings?")
        subtitle: i18np("The selected setting will be overwritten in every project. This cannot be undone.",
                        "The %1 selected settings will be overwritten in every project. This cannot be undone.",
                        dialog.selectedCount)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                icon.name: "dialog-ok-apply"
                text: i18nc("@action:button", "Apply")
                onTriggered: {
                    App.applyToAllProjects(Object.keys(dialog.selected).filter(k => dialog.selected[k]));
                    confirm.close();
                    dialog.close();
                }
            }
        ]
    }
}
