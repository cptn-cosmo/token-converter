// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KD
import org.kde.tokenconverter

ColumnLayout {
    id: view

    required property JobRunner runner

    spacing: Kirigami.Units.smallSpacing

    function statusIcon(status: string): string {
        switch (status) {
        case "converted":
            return "emblem-success";
        case "skipped":
            return "media-skip-forward";
        case "failed":
            return "emblem-error";
        }
        return "unknown";
    }

    function statusLabel(status: string): string {
        switch (status) {
        case "converted":
            return i18nc("@info:status file", "Converted");
        case "skipped":
            return i18nc("@info:status file", "Skipped");
        case "failed":
            return i18nc("@info:status file", "Failed");
        }
        return status;
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            text: i18nc("@label filter results by status", "Show:")
        }

        QQC2.ButtonGroup {
            id: filterGroup
        }

        Repeater {
            model: [
                { filter: "", label: i18nc("@option:radio", "All (%1)", view.runner.convertedCount + view.runner.skippedCount + view.runner.failedCount) },
                { filter: "converted", label: i18nc("@option:radio", "Converted (%1)", view.runner.convertedCount) },
                { filter: "skipped", label: i18nc("@option:radio", "Skipped (%1)", view.runner.skippedCount) },
                { filter: "failed", label: i18nc("@option:radio", "Failed (%1)", view.runner.failedCount) }
            ]
            delegate: QQC2.Button {
                required property var modelData
                text: modelData.label
                checkable: true
                flat: !checked
                checked: view.runner.results.statusFilter === modelData.filter
                QQC2.ButtonGroup.group: filterGroup
                onClicked: view.runner.results.statusFilter = modelData.filter
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true

        background: Rectangle {
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, Kirigami.Theme.frameContrast)
            radius: Kirigami.Units.cornerRadius
        }

        ListView {
            id: list
            model: view.runner.results
            clip: true
            activeFocusOnTab: true
            keyNavigationEnabled: true
            reuseItems: true
            Accessible.name: i18nc("@info:accessible", "Conversion results")
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false

            delegate: QQC2.ItemDelegate {
                id: resultDelegate

                required property string relativePath
                required property string targetName
                required property string status
                required property double sizeBefore
                required property double sizeAfter
                required property string message

                width: ListView.view.width
                Accessible.name: view.statusLabel(status) + ": " + relativePath
                Accessible.description: message

                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.Icon {
                        source: view.statusIcon(resultDelegate.status)
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        Accessible.name: view.statusLabel(resultDelegate.status)
                    }

                    KD.TitleSubtitle {
                        Layout.fillWidth: true
                        title: resultDelegate.targetName.length > 0
                               ? resultDelegate.relativePath + " → " + resultDelegate.targetName
                               : resultDelegate.relativePath
                        subtitle: resultDelegate.message.length > 0 ? resultDelegate.message : view.statusLabel(resultDelegate.status)
                        elide: Text.ElideMiddle
                    }

                    QQC2.Label {
                        visible: resultDelegate.status === "converted"
                        text: i18nc("@info size before → after", "%1 → %2",
                                    Qt.locale().formattedDataSize(resultDelegate.sizeBefore, 1),
                                    Qt.locale().formattedDataSize(resultDelegate.sizeAfter, 1))
                        opacity: 0.8
                    }
                }
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 2
                visible: list.count === 0
                text: view.runner.results.statusFilter.length > 0 ? i18nc("@info:placeholder", "No files with this status")
                                                                  : i18nc("@info:placeholder", "No results yet")
            }
        }
    }
}
