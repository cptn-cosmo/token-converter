// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.tokenconverter

import "SettingLabels.js" as Labels

/// Small dot + reset button shown next to a setting that differs from its
/// reference (global defaults for projects, factory defaults for the global page).
RowLayout {
    id: marker

    required property SettingsController controller
    required property string key

    readonly property bool modified: controller.modifiedKeys.indexOf(key) >= 0

    spacing: Kirigami.Units.smallSpacing
    visible: modified

    Rectangle {
        Layout.alignment: Qt.AlignVCenter
        implicitWidth: Kirigami.Units.smallSpacing * 2
        implicitHeight: implicitWidth
        radius: width / 2
        color: Kirigami.Theme.highlightColor
        Accessible.role: Accessible.Indicator
        Accessible.name: marker.controller.isGlobal ? i18nc("@info:accessible", "Differs from factory default")
                                                    : i18nc("@info:accessible", "Differs from global default")
    }

    QQC2.ToolButton {
        icon.name: "edit-undo"
        text: marker.controller.isGlobal ? i18nc("@action:button", "Reset “%1” to factory default", Labels.label(marker.key))
                                         : i18nc("@action:button", "Reset “%1” to global default", Labels.label(marker.key))
        display: QQC2.AbstractButton.IconOnly
        onClicked: marker.controller.resetValue(marker.key)
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered || activeFocus
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        Accessible.name: text
    }
}
