// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.tokenconverter

Kirigami.Dialog {
    id: dialog

    required property JobRunner runner
    property bool decided: false

    title: i18nc("@title:window", "Preview Conversion")
    preferredWidth: Math.min(Kirigami.Units.gridUnit * 56, QQC2.ApplicationWindow.window ? QQC2.ApplicationWindow.window.width - Kirigami.Units.gridUnit * 2 : Kirigami.Units.gridUnit * 56)
    preferredHeight: Kirigami.Units.gridUnit * 30
    padding: 0
    closePolicy: QQC2.Popup.CloseOnEscape

    onOpened: decided = false
    // Closing the dialog any other way counts as cancel: nothing is touched.
    onClosed: {
        if (!decided) {
            runner.rejectPreview();
        }
    }

    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            icon.name: "dialog-ok-apply"
            text: i18ncp("@action:button", "Convert %1 File", "Convert %1 Files", dialog.runner.preview.convertCount)
            enabled: dialog.runner.preview.convertCount > 0
            onTriggered: {
                dialog.decided = true;
                dialog.runner.confirmPreview();
                dialog.close();
            }
        },
        Kirigami.Action {
            icon.name: "dialog-cancel"
            text: i18nc("@action:button", "Cancel")
            onTriggered: {
                dialog.decided = true;
                dialog.runner.rejectPreview();
                dialog.close();
            }
        }
    ]

    ColumnLayout {
        spacing: 0

        QQC2.Label {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            wrapMode: Text.Wrap
            text: dialog.runner.preview.count === 0
                  ? i18n("No matching files were found. Nothing will be changed.")
                  : i18np("%1 file matched. Nothing has been changed yet.", "%1 files matched. Nothing has been changed yet.", dialog.runner.preview.count)
        }

        // Column headers
        RowLayout {
            id: headerRow
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing
            visible: dialog.runner.preview.count > 0

            Repeater {
                model: [i18nc("@title:column", "File"), i18nc("@title:column", "Target name"), i18nc("@title:column", "Estimated result"), i18nc("@title:column", "Action")]
                delegate: QQC2.Label {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredWidth: index === 2 ? 3 : 2
                    text: modelData
                    font.bold: true
                    elide: Text.ElideRight
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
        }

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Kirigami.Units.gridUnit * 12

            ListView {
                model: dialog.runner.preview
                clip: true
                activeFocusOnTab: true
                keyNavigationEnabled: true
                reuseItems: true
                Accessible.name: i18nc("@info:accessible", "Preview of planned conversions")

                delegate: QQC2.ItemDelegate {
                    id: row

                    required property string relativePath
                    required property string targetName
                    required property string estimate
                    required property string originalAction
                    required property bool willConvert

                    width: ListView.view.width
                    Accessible.name: relativePath + ", " + targetName + ", " + estimate + ", " + originalAction

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        opacity: row.willConvert ? 1 : 0.6

                        QQC2.Label {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 2
                            text: row.relativePath
                            elide: Text.ElideMiddle
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 2
                            text: row.targetName
                            elide: Text.ElideMiddle
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 3
                            text: row.estimate
                            elide: Text.ElideRight
                            color: row.willConvert ? Kirigami.Theme.textColor : Kirigami.Theme.neutralTextColor
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 2
                            text: row.originalAction
                            elide: Text.ElideRight
                        }
                    }

                    QQC2.ToolTip.text: relativePath + "\n→ " + targetName + "\n" + estimate + "\n" + originalAction
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }
        }
    }
}
