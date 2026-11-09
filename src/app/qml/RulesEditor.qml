// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.tokenconverter

/// Editable list of "source extensions -> target format" rules.
ColumnLayout {
    id: editor

    required property SettingsController controller
    readonly property var rules: controller.values.rules

    spacing: Kirigami.Units.smallSpacing

    function cloneRules() {
        return rules.map(r => ({ sources: r.sources.slice(), target: r.target }));
    }

    function setSources(index, text) {
        const next = cloneRules();
        next[index].sources = text.split(/[,;\s]+/).filter(s => s.length > 0);
        controller.setValue("rules", next);
    }

    function setTarget(index, text) {
        const next = cloneRules();
        next[index].target = text;
        controller.setValue("rules", next);
    }

    function removeRule(index) {
        const next = cloneRules();
        next.splice(index, 1);
        controller.setValue("rules", next);
    }

    function addRule() {
        const next = cloneRules();
        next.push({ sources: ["jpg", "jpeg"], target: "webp" });
        controller.setValue("rules", next);
    }

    Repeater {
        model: editor.rules

        delegate: RowLayout {
            id: ruleRow

            required property var modelData
            required property int index

            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: sourcesField
                Layout.fillWidth: true
                Layout.minimumWidth: Kirigami.Units.gridUnit * 8
                text: ruleRow.modelData.sources.join(", ")
                placeholderText: i18nc("@info:placeholder", "e.g. png, jpg")
                Accessible.name: i18nc("@label:textbox", "Source extensions of rule %1", ruleRow.index + 1)
                onEditingFinished: {
                    if (text !== ruleRow.modelData.sources.join(", ")) {
                        editor.setSources(ruleRow.index, text);
                    }
                }
            }

            Kirigami.Icon {
                source: Qt.application.layoutDirection === Qt.RightToLeft ? "arrow-left" : "arrow-right"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                Accessible.ignored: true
            }

            QQC2.ComboBox {
                id: targetCombo
                Layout.preferredWidth: Kirigami.Units.gridUnit * 7
                editable: true
                model: editor.controller.targetFormats
                Accessible.name: i18nc("@label:listbox", "Target format of rule %1", ruleRow.index + 1)
                Component.onCompleted: {
                    const i = find(ruleRow.modelData.target);
                    currentIndex = i;
                    editText = ruleRow.modelData.target;
                }
                onActivated: editor.setTarget(ruleRow.index, currentText)
                onAccepted: editor.setTarget(ruleRow.index, editText)
            }

            QQC2.ToolButton {
                icon.name: "list-remove"
                text: i18nc("@action:button", "Remove rule %1", ruleRow.index + 1)
                display: QQC2.AbstractButton.IconOnly
                enabled: editor.rules.length > 1
                onClicked: editor.removeRule(ruleRow.index)
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                Accessible.name: text
            }
        }
    }

    QQC2.Button {
        icon.name: "list-add"
        text: i18nc("@action:button", "Add Rule")
        onClicked: editor.addRule()
    }
}
