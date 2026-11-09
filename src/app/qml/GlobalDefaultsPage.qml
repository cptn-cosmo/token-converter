// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.tokenconverter

Kirigami.ScrollablePage {
    id: page

    title: i18nc("@title", "Global Defaults")

    actions: [
        Kirigami.Action {
            icon.name: "document-send"
            text: i18nc("@action:button", "Apply to All Projects…")
            enabled: App.projects.count > 0
            onTriggered: applyDialog.open()
        },
        Kirigami.Action {
            icon.name: "edit-reset"
            text: i18nc("@action:button", "Restore Factory Defaults…")
            enabled: App.globalSettings.modifiedKeys.length > 0
            onTriggered: factoryDialog.open()
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: i18n("New projects start with a copy of these settings. Changing them does not affect existing projects.")
        }

        SettingsForm {
            Layout.fillWidth: true
            controller: App.globalSettings
        }
    }

    Kirigami.PromptDialog {
        id: factoryDialog
        title: i18nc("@title:window", "Restore Factory Defaults")
        subtitle: i18n("Replace the global defaults with the built-in factory defaults? Existing projects are not changed.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                icon.name: "edit-reset"
                text: i18nc("@action:button", "Restore")
                onTriggered: {
                    App.restoreFactoryDefaults();
                    factoryDialog.close();
                }
            }
        ]
    }

    ApplyToAllDialog {
        id: applyDialog
    }
}
