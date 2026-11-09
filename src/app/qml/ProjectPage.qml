// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.tokenconverter

Kirigami.ScrollablePage {
    id: page

    required property ProjectSession session

    title: session.name

    actions: [
        Kirigami.Action {
            icon.name: "document-open-folder"
            text: i18nc("@action:button", "Open Folder")
            onTriggered: App.openFolder(page.session.projectId)
        },
        Kirigami.Action {
            icon.name: "edit-reset"
            text: i18nc("@action:button", "Reset to Global Defaults…")
            enabled: page.session.settings.modifiedKeys.length > 0 && !page.session.runner.busy
            onTriggered: resetDialog.open()
        }
    ]

    header: ProgressHeader {
        session: page.session
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            visible: !page.session.folderExists
            text: i18n("The project folder “%1” does not exist anymore.", page.session.path)
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.session.settings.modifiedKeys.length > 0
            type: Kirigami.MessageType.Information
            text: i18np("One setting differs from the global defaults (marked with a dot).",
                        "%1 settings differ from the global defaults (marked with a dot).",
                        page.session.settings.modifiedKeys.length)
        }

        SettingsForm {
            Layout.fillWidth: true
            controller: page.session.settings
            enabled: !page.session.runner.busy
        }
    }

    Kirigami.PromptDialog {
        id: resetDialog
        title: i18nc("@title:window", "Reset to Global Defaults")
        subtitle: i18n("Replace all settings of “%1” with the current global defaults? This cannot be undone.", page.session.name)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                icon.name: "edit-reset"
                text: i18nc("@action:button", "Reset")
                onTriggered: {
                    page.session.settings.resetAll();
                    resetDialog.close();
                }
            }
        ]
    }
}
