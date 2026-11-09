// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page

    signal addProjectRequested()

    title: i18nc("@title", "Welcome")

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        icon.name: "token-converter"
        text: i18nc("@info:placeholder", "Add a project folder to get started")
        explanation: i18n("Token Converter recursively converts media files inside your project folders, for example PNG to WebP. Originals are only touched after a conversion was verified.")
        helpfulAction: Kirigami.Action {
            icon.name: "list-add"
            text: i18nc("@action:button", "Add Project…")
            onTriggered: page.addProjectRequested()
        }
    }
}
