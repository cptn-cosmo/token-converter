// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KD
import org.kde.tokenconverter

Kirigami.OverlayDrawer {
    id: drawer

    required property Kirigami.ApplicationWindow window
    property string currentView

    signal projectSelected(string id)
    signal globalSelected()
    signal addProjectRequested()
    signal projectRemoved(string id)

    edge: Qt.application.layoutDirection === Qt.RightToLeft ? Qt.RightEdge : Qt.LeftEdge
    modal: !window.wideScreen
    onModalChanged: drawerOpen = !modal
    drawerOpen: !modal
    handleVisible: modal
    width: Kirigami.Units.gridUnit * 16
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    contentItem: ColumnLayout {
        spacing: 0

        QQC2.ToolBar {
            Layout.fillWidth: true
            Layout.preferredHeight: drawer.window.pageStack.globalToolBar.preferredHeight

            RowLayout {
                anchors.fill: parent
                Kirigami.Heading {
                    level: 1
                    text: i18nc("@title", "Projects")
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    elide: Text.ElideRight
                }
                QQC2.ToolButton {
                    icon.name: "list-add"
                    text: i18nc("@action:button", "Add Project…")
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: drawer.addProjectRequested()
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Accessible.name: text
                }
            }
        }

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: projectList
                model: App.projects
                clip: true
                keyNavigationEnabled: true
                activeFocusOnTab: true
                Accessible.name: i18nc("@info:accessible", "Projects")
                Accessible.role: Accessible.List

                Keys.onReturnPressed: if (currentItem) currentItem.clicked()
                Keys.onEnterPressed: if (currentItem) currentItem.clicked()
                Keys.onMenuPressed: if (currentItem) currentItem.openMenu()

                delegate: QQC2.ItemDelegate {
                    id: projectDelegate

                    required property int index
                    required property string projectId
                    required property string name
                    required property string path
                    required property ProjectSession session

                    readonly property JobRunner runner: session ? session.runner : null

                    width: ListView.view.width
                    highlighted: drawer.currentView === projectId
                    text: name
                    Accessible.name: name
                    Accessible.description: path

                    onClicked: {
                        ListView.view.currentIndex = index;
                        drawer.projectSelected(projectId);
                    }
                    function openMenu() {
                        contextMenu.popup(projectDelegate, 0, projectDelegate.height);
                    }

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: contextMenu.popup()
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "folder"
                            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        }

                        KD.TitleSubtitle {
                            Layout.fillWidth: true
                            title: projectDelegate.name
                            subtitle: projectDelegate.runner && projectDelegate.runner.busy
                                      ? i18nc("@info:status", "%1 of %2 files", projectDelegate.runner.processed, projectDelegate.runner.total)
                                      : projectDelegate.path
                            selected: projectDelegate.highlighted || projectDelegate.down
                            elide: Text.ElideMiddle
                        }

                        QQC2.BusyIndicator {
                            visible: projectDelegate.runner !== null && projectDelegate.runner.busy
                            running: visible
                            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        }

                        QQC2.ToolButton {
                            id: menuButton
                            icon.name: "overflow-menu"
                            text: i18nc("@action:button", "Project Actions")
                            display: QQC2.AbstractButton.IconOnly
                            onClicked: contextMenu.popup(menuButton, 0, menuButton.height)
                            QQC2.ToolTip.text: text
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            Accessible.name: text
                        }
                    }

                    QQC2.Menu {
                        id: contextMenu
                        QQC2.MenuItem {
                            icon.name: "document-open-folder"
                            text: i18nc("@action:inmenu", "Open Folder")
                            onTriggered: App.openFolder(projectDelegate.projectId)
                        }
                        QQC2.MenuItem {
                            icon.name: "edit-rename"
                            text: i18nc("@action:inmenu", "Rename…")
                            onTriggered: {
                                renameDialog.projectId = projectDelegate.projectId;
                                renameDialog.field.text = projectDelegate.name;
                                renameDialog.open();
                            }
                        }
                        QQC2.MenuSeparator {}
                        QQC2.MenuItem {
                            icon.name: "list-remove"
                            text: i18nc("@action:inmenu", "Remove…")
                            onTriggered: {
                                removeDialog.projectId = projectDelegate.projectId;
                                removeDialog.projectName = projectDelegate.name;
                                removeDialog.open();
                            }
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 2
                    visible: projectList.count === 0
                    icon.name: "folder-new"
                    text: i18nc("@info:placeholder", "No projects yet")
                    helpfulAction: Kirigami.Action {
                        icon.name: "list-add"
                        text: i18nc("@action:button", "Add Project…")
                        onTriggered: drawer.addProjectRequested()
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        QQC2.ItemDelegate {
            Layout.fillWidth: true
            icon.name: "configure"
            text: i18nc("@action:button", "Global Defaults")
            highlighted: drawer.currentView === "global"
            onClicked: drawer.globalSelected()
            Accessible.name: text
        }
    }

    Kirigami.PromptDialog {
        id: renameDialog
        property string projectId
        property alias field: renameField
        parent: drawer.window.QQC2.Overlay.overlay
        title: i18nc("@title:window", "Rename Project")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onOpened: {
            renameField.forceActiveFocus();
            renameField.selectAll();
        }
        onAccepted: App.renameProject(projectId, renameField.text)

        QQC2.TextField {
            id: renameField
            Accessible.name: i18nc("@label:textbox", "Project name")
            onAccepted: renameDialog.accept()
        }
    }

    Kirigami.PromptDialog {
        id: removeDialog
        property string projectId
        property string projectName
        parent: drawer.window.QQC2.Overlay.overlay
        title: i18nc("@title:window", "Remove Project")
        subtitle: i18n("Remove “%1” from the list? No files will be deleted.", projectName)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                icon.name: "list-remove"
                text: i18nc("@action:button", "Remove")
                onTriggered: {
                    const id = removeDialog.projectId;
                    removeDialog.close();
                    App.removeProject(id);
                    drawer.projectRemoved(id);
                }
            }
        ]
    }
}
