// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.tokenconverter

/// The settings form, used for both a project and the global defaults page.
Kirigami.FormLayout {
    id: form

    required property SettingsController controller
    readonly property var v: controller.values

    readonly property var originalModes: [
        { value: "replace", text: i18nc("@item:inlistbox original files", "Replace original") },
        { value: "trash", text: i18nc("@item:inlistbox original files", "Move original to Trash") },
        { value: "backup", text: i18nc("@item:inlistbox original files", "Move original to a backup folder") },
        { value: "keep", text: i18nc("@item:inlistbox original files", "Keep both") }
    ]
    readonly property var collisionModes: [
        { value: "skip", text: i18nc("@item:inlistbox name collision", "Skip the file") },
        { value: "overwrite", text: i18nc("@item:inlistbox name collision", "Overwrite the existing file") },
        { value: "autonumber", text: i18nc("@item:inlistbox name collision", "Auto-number (name-1, name-2, …)") }
    ]

    function listText(list) {
        return list ? list.join(", ") : "";
    }

    wideMode: width > Kirigami.Units.gridUnit * 30

    // ------------------------------------------------------------- basic

    Item {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nc("@title:group", "Conversion")
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label", "Rules:")
        Kirigami.FormData.labelAlignment: Qt.AlignTop
        Layout.fillWidth: true
        Layout.maximumWidth: Kirigami.Units.gridUnit * 28

        RulesEditor {
            Layout.fillWidth: true
            controller: form.controller
        }
        ModifiedMarker {
            Layout.alignment: Qt.AlignTop
            controller: form.controller
            key: "rules"
        }
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label", "Quality:")
        QQC2.CheckBox {
            text: i18nc("@option:check", "Lossless")
            checked: form.v.lossless
            onToggled: form.controller.setValue("lossless", checked)
        }
        ModifiedMarker {
            controller: form.controller
            key: "lossless"
        }
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label:slider", "Lossy quality:")
        enabled: !form.v.lossless
        QQC2.Slider {
            id: qualitySlider
            Layout.preferredWidth: Kirigami.Units.gridUnit * 12
            from: 0
            to: 100
            stepSize: 1
            value: form.v.quality
            onMoved: form.controller.setValue("quality", Math.round(value))
            Accessible.name: i18nc("@label:slider", "Lossy quality")
        }
        QQC2.SpinBox {
            from: 0
            to: 100
            value: form.v.quality
            editable: true
            onValueModified: form.controller.setValue("quality", value)
            Accessible.name: i18nc("@label:spinbox", "Lossy quality")
        }
        ModifiedMarker {
            controller: form.controller
            key: "quality"
        }
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label:listbox", "Original files:")
        QQC2.ComboBox {
            model: form.originalModes
            textRole: "text"
            valueRole: "value"
            currentIndex: form.originalModes.findIndex(m => m.value === form.v.originalMode)
            onActivated: form.controller.setValue("originalMode", currentValue)
            Accessible.name: i18nc("@label:listbox", "Original files")
        }
        ModifiedMarker {
            controller: form.controller
            key: "originalMode"
        }
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label", "Safety:")
        QQC2.CheckBox {
            text: i18nc("@option:check", "Preview before converting (dry run)")
            checked: form.v.dryRun
            onToggled: form.controller.setValue("dryRun", checked)
        }
        ModifiedMarker {
            controller: form.controller
            key: "dryRun"
        }
    }

    RowLayout {
        QQC2.CheckBox {
            text: i18nc("@option:check", "Skip file if the converted result is larger")
            checked: form.v.skipIfLarger
            onToggled: form.controller.setValue("skipIfLarger", checked)
        }
        ModifiedMarker {
            controller: form.controller
            key: "skipIfLarger"
        }
    }

    QQC2.Switch {
        Kirigami.FormData.label: i18nc("@label", "Advanced:")
        text: i18nc("@option:check", "Show advanced settings")
        checked: form.controller.advancedVisible
        onToggled: form.controller.advancedVisible = checked
    }

    // ----------------------------------------------------------- advanced

    Item {
        visible: form.controller.advancedVisible
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nc("@title:group", "Filtering")
    }

    RowLayout {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label:textbox", "Include patterns:")
        QQC2.TextField {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 18
            text: form.listText(form.v.includePatterns)
            placeholderText: i18nc("@info:placeholder", "All files (e.g. assets/**, *.png)")
            onEditingFinished: form.controller.setValue("includePatterns", text)
            Accessible.name: i18nc("@label:textbox", "Include patterns")
        }
        ModifiedMarker {
            controller: form.controller
            key: "includePatterns"
        }
    }

    RowLayout {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label:textbox", "Exclude patterns:")
        QQC2.TextField {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 18
            text: form.listText(form.v.excludePatterns)
            placeholderText: i18nc("@info:placeholder", "e.g. *_thumb.png, raw/**")
            onEditingFinished: form.controller.setValue("excludePatterns", text)
            Accessible.name: i18nc("@label:textbox", "Exclude patterns")
        }
        ModifiedMarker {
            controller: form.controller
            key: "excludePatterns"
        }
    }

    RowLayout {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label:textbox", "Excluded folders:")
        QQC2.TextField {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 18
            text: form.listText(form.v.excludeFolders)
            placeholderText: i18nc("@info:placeholder", "e.g. .git, node_modules")
            onEditingFinished: form.controller.setValue("excludeFolders", text)
            Accessible.name: i18nc("@label:textbox", "Excluded folders")
        }
        ModifiedMarker {
            controller: form.controller
            key: "excludeFolders"
        }
    }

    QQC2.Label {
        visible: form.controller.advancedVisible
        Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        wrapMode: Text.Wrap
        font: Kirigami.Theme.smallFont
        opacity: 0.8
        text: i18n("Comma-separated globs: * matches within a folder, ** across folders. Patterns without “/” match file names, others match paths relative to the project folder.")
    }

    RowLayout {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label", "Symlinks:")
        QQC2.CheckBox {
            text: i18nc("@option:check", "Follow symbolic links")
            checked: form.v.followSymlinks
            onToggled: form.controller.setValue("followSymlinks", checked)
        }
        ModifiedMarker {
            controller: form.controller
            key: "followSymlinks"
        }
    }

    Item {
        visible: form.controller.advancedVisible
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nc("@title:group", "Renaming")
    }

    RowLayout {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label:textbox", "Prefix:")
        QQC2.TextField {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 12
            text: form.v.prefix
            onTextEdited: form.controller.setValue("prefix", text)
            Accessible.name: i18nc("@label:textbox", "Prefix")
        }
        ModifiedMarker {
            controller: form.controller
            key: "prefix"
        }
    }

    RowLayout {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label:textbox", "Postfix:")
        QQC2.TextField {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 12
            text: form.v.postfix
            onTextEdited: form.controller.setValue("postfix", text)
            Accessible.name: i18nc("@label:textbox", "Postfix")
        }
        ModifiedMarker {
            controller: form.controller
            key: "postfix"
        }
    }

    RowLayout {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label:textbox", "Filename template:")
        QQC2.TextField {
            id: templateField
            Layout.preferredWidth: Kirigami.Units.gridUnit * 18
            text: form.v.filenameTemplate
            placeholderText: "{name}"
            onTextEdited: form.controller.setValue("filenameTemplate", text)
            Accessible.name: i18nc("@label:textbox", "Filename template")
        }
        ModifiedMarker {
            controller: form.controller
            key: "filenameTemplate"
        }
    }

    Flow {
        visible: form.controller.advancedVisible
        Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        spacing: Kirigami.Units.smallSpacing

        Repeater {
            model: ["{name}", "{ext}", "{date}", "{time}", "{counter}", "{parent}", "{width}", "{height}"]
            delegate: QQC2.Button {
                required property string modelData
                text: modelData
                flat: true
                font: Kirigami.Theme.smallFont
                Accessible.name: i18nc("@action:button", "Insert placeholder %1", modelData)
                onClicked: {
                    templateField.insert(templateField.cursorPosition, modelData);
                    form.controller.setValue("filenameTemplate", templateField.text);
                }
            }
        }
    }

    QQC2.Label {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label", "Preview:")
        Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        wrapMode: Text.WrapAnywhere
        text: {
            form.v; // re-evaluate on every settings change
            const example = form.controller.exampleFileName();
            return example + "  →  " + form.controller.previewFileName(example);
        }
        font.family: "monospace"
        Accessible.name: i18nc("@info:accessible", "Filename preview: %1", text)
    }

    RowLayout {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label:listbox", "If the name exists:")
        QQC2.ComboBox {
            model: form.collisionModes
            textRole: "text"
            valueRole: "value"
            currentIndex: form.collisionModes.findIndex(m => m.value === form.v.collisionMode)
            onActivated: form.controller.setValue("collisionMode", currentValue)
            Accessible.name: i18nc("@label:listbox", "If the name exists")
        }
        ModifiedMarker {
            controller: form.controller
            key: "collisionMode"
        }
    }

    Item {
        visible: form.controller.advancedVisible
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nc("@title:group", "Processing")
    }

    RowLayout {
        visible: form.controller.advancedVisible
        enabled: form.v.originalMode === "backup"
        Kirigami.FormData.label: i18nc("@label:textbox", "Backup folder:")
        QQC2.TextField {
            id: backupField
            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
            text: form.v.backupFolder
            onEditingFinished: form.controller.setValue("backupFolder", text)
            Accessible.name: i18nc("@label:textbox", "Backup folder")
            QQC2.ToolTip.text: i18n("Relative paths are inside the project folder. The backup folder is never scanned.")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
        QQC2.Button {
            icon.name: "document-open-folder"
            text: i18nc("@action:button", "Browse…")
            display: QQC2.AbstractButton.IconOnly
            onClicked: backupDialog.open()
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            Accessible.name: i18nc("@action:button", "Choose backup folder")
        }
        ModifiedMarker {
            controller: form.controller
            key: "backupFolder"
        }
    }

    RowLayout {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label:spinbox", "Parallel workers:")
        QQC2.SpinBox {
            from: 1
            to: 256
            value: form.v.workerCount
            editable: true
            onValueModified: form.controller.setValue("workerCount", value)
            Accessible.name: i18nc("@label:spinbox", "Parallel workers")
        }
        QQC2.Label {
            text: i18nc("@info", "(%1 CPU cores)", form.controller.idealWorkerCount)
            opacity: 0.8
        }
        ModifiedMarker {
            controller: form.controller
            key: "workerCount"
        }
    }

    RowLayout {
        visible: form.controller.advancedVisible
        Kirigami.FormData.label: i18nc("@label", "Metadata:")
        QQC2.CheckBox {
            text: i18nc("@option:check", "Preserve metadata (EXIF, ICC color profile)")
            checked: form.v.preserveMetadata
            onToggled: form.controller.setValue("preserveMetadata", checked)
        }
        ModifiedMarker {
            controller: form.controller
            key: "preserveMetadata"
        }
    }

    FolderDialog {
        id: backupDialog
        title: i18nc("@title:window", "Choose Backup Folder")
        onAccepted: form.controller.setValue("backupFolder", App.localPath(selectedFolder))
    }
}
