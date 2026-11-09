// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.tokenconverter

Kirigami.ApplicationWindow {
    id: root

    title: i18nc("@title:window", "Token Converter")
    width: Kirigami.Units.gridUnit * 64
    height: Kirigami.Units.gridUnit * 44
    minimumWidth: Kirigami.Units.gridUnit * 22
    minimumHeight: Kirigami.Units.gridUnit * 20

    // Either a project id, "global", or "" (welcome page)
    property string currentView: ""

    function showProject(id: string) {
        if (currentView === id && pageStack.depth > 0) {
            return;
        }
        const session = App.session(id);
        if (!session) {
            showWelcome();
            return;
        }
        currentView = id;
        pageStack.clear();
        pageStack.push(projectPageComponent, { session: session });
        if (sidebar.modal) {
            sidebar.close();
        }
    }

    function showGlobal() {
        if (currentView === "global" && pageStack.depth > 0) {
            return;
        }
        currentView = "global";
        pageStack.clear();
        pageStack.push(globalPageComponent);
        if (sidebar.modal) {
            sidebar.close();
        }
    }

    function showWelcome() {
        currentView = "";
        pageStack.clear();
        pageStack.push(welcomePageComponent);
    }

    function addProject() {
        folderDialog.open();
    }

    globalDrawer: Sidebar {
        id: sidebar
        window: root
        currentView: root.currentView
        onProjectSelected: id => root.showProject(id)
        onGlobalSelected: root.showGlobal()
        onAddProjectRequested: root.addProject()
        onProjectRemoved: id => {
            if (root.currentView === id) {
                if (App.projects.count > 0) {
                    root.showProject(App.projectIdAt(0));
                } else {
                    root.showWelcome();
                }
            }
        }
    }

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.ToolBar
    pageStack.globalToolBar.showNavigationButtons: Kirigami.ApplicationHeaderStyle.NoNavigationButtons
    pageStack.columnView.columnResizeMode: Kirigami.ColumnView.SingleColumn

    Component.onCompleted: {
        if (App.projects.count > 0) {
            showProject(App.projectIdAt(0));
        } else {
            showWelcome();
        }
    }

    Component {
        id: welcomePageComponent
        WelcomePage {
            onAddProjectRequested: root.addProject()
        }
    }

    Component {
        id: projectPageComponent
        ProjectPage {}
    }

    Component {
        id: globalPageComponent
        GlobalDefaultsPage {}
    }

    FolderDialog {
        id: folderDialog
        title: i18nc("@title:window", "Choose Project Folder")
        onAccepted: {
            addProjectDialog.folder = selectedFolder;
            addProjectDialog.nameField.text = App.folderName(selectedFolder);
            addProjectDialog.open();
        }
    }

    Kirigami.Dialog {
        id: addProjectDialog

        property url folder
        property alias nameField: nameField

        title: i18nc("@title:window", "Add Project")
        padding: Kirigami.Units.largeSpacing
        preferredWidth: Kirigami.Units.gridUnit * 24
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

        onOpened: {
            nameField.forceActiveFocus();
            nameField.selectAll();
        }
        onAccepted: {
            const id = App.addProject(folder, nameField.text);
            if (id.length > 0) {
                root.showProject(id);
            } else {
                root.showPassiveNotification(i18n("The folder could not be added."));
            }
        }

        Kirigami.FormLayout {
            QQC2.TextField {
                id: nameField
                Kirigami.FormData.label: i18nc("@label:textbox", "Name:")
                onAccepted: addProjectDialog.accept()
            }
            QQC2.Label {
                Kirigami.FormData.label: i18nc("@label", "Folder:")
                text: App.localPath(addProjectDialog.folder)
                elide: Text.ElideMiddle
                Layout.maximumWidth: Kirigami.Units.gridUnit * 16
            }
        }
    }
}
