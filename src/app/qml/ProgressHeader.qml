// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.tokenconverter

QQC2.Pane {
    id: header

    required property ProjectSession session
    readonly property JobRunner runner: session.runner
    property bool resultsExpanded: false

    readonly property bool isRunning: runner.state === JobRunner.Running
    readonly property bool isPaused: runner.state === JobRunner.Paused || runner.state === JobRunner.Pausing
    readonly property bool hasRun: runner.total > 0 || runner.state === JobRunner.Finished || runner.state === JobRunner.Cancelled

    padding: Kirigami.Units.largeSpacing
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
        Kirigami.Separator {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
        }
    }

    function stateText(): string {
        switch (runner.state) {
        case JobRunner.Idle:
            return i18nc("@info:status", "Ready");
        case JobRunner.Scanning:
            return i18ncp("@info:status", "Scanning… %1 file seen", "Scanning… %1 files seen", runner.scannedFiles);
        case JobRunner.AwaitingConfirmation:
            return i18nc("@info:status", "Waiting for confirmation of the preview");
        case JobRunner.Running:
            return i18nc("@info:status", "%1 of %2 files", runner.processed, runner.total);
        case JobRunner.Pausing:
            return i18nc("@info:status", "Pausing after the current files… (%1 of %2)", runner.processed, runner.total);
        case JobRunner.Paused:
            return i18nc("@info:status", "Paused — %1 of %2 files", runner.processed, runner.total);
        case JobRunner.Cancelling:
            return i18nc("@info:status", "Cancelling…");
        case JobRunner.Finished:
            return runner.total === 0 ? i18nc("@info:status", "Done — no matching files found")
                                      : i18nc("@info:status", "Done — %1 of %2 files", runner.processed, runner.total);
        case JobRunner.Cancelled:
            return i18nc("@info:status", "Cancelled — %1 of %2 files", runner.processed, runner.total);
        }
        return "";
    }

    Connections {
        target: header.runner
        function onStateChanged() {
            if (header.runner.state === JobRunner.AwaitingConfirmation) {
                previewDialog.open();
            } else if (header.runner.state === JobRunner.Finished && header.runner.failedCount > 0) {
                header.resultsExpanded = true;
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Kirigami.Heading {
                    level: 3
                    text: header.stateText()
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    Accessible.role: Accessible.StaticText
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: header.runner.currentFile
                    visible: text.length > 0 && header.runner.busy
                    elide: Text.ElideMiddle
                    opacity: 0.8
                }
            }

            QQC2.Button {
                icon.name: "media-playback-start"
                text: i18nc("@action:button", "Start")
                visible: !header.runner.busy
                enabled: header.session.folderExists
                onClicked: header.session.start()
            }
            QQC2.Button {
                icon.name: "media-playback-pause"
                text: i18nc("@action:button", "Pause")
                visible: header.isRunning
                onClicked: header.runner.pause()
            }
            QQC2.Button {
                icon.name: "media-playback-start"
                text: i18nc("@action:button", "Resume")
                visible: header.isPaused
                onClicked: header.runner.resume()
            }
            QQC2.Button {
                icon.name: "dialog-cancel"
                text: i18nc("@action:button", "Cancel")
                visible: header.runner.busy
                enabled: header.runner.state !== JobRunner.Cancelling
                onClicked: header.runner.cancel()
            }
        }

        QQC2.ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 1
            value: header.runner.progress
            indeterminate: header.runner.state === JobRunner.Scanning
            Accessible.name: header.stateText()
        }

        RowLayout {
            Layout.fillWidth: true
            visible: header.hasRun
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: header.runner.bytesBefore > 0
                      ? i18nc("@info saved bytes", "Saved %1 (%2%) of %3",
                              Qt.locale().formattedDataSize(header.runner.savedBytes, 1),
                              header.runner.savedPercent.toFixed(1),
                              Qt.locale().formattedDataSize(header.runner.bytesBefore, 1))
                      : i18nc("@info", "No space saved yet")
            }

            QQC2.Button {
                flat: true
                icon.name: header.resultsExpanded ? "arrow-up" : "arrow-down"
                text: i18nc("@action:button", "Results (%1)", header.runner.convertedCount + header.runner.skippedCount + header.runner.failedCount)
                checkable: true
                checked: header.resultsExpanded
                onToggled: header.resultsExpanded = checked
                Accessible.name: header.resultsExpanded ? i18nc("@action:button", "Hide results") : i18nc("@action:button", "Show results")
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: header.runner.errorString.length > 0
            text: header.runner.errorString
        }

        ResultsView {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 14
            visible: header.resultsExpanded
            runner: header.runner
        }
    }

    PreviewDialog {
        id: previewDialog
        runner: header.runner
    }
}
