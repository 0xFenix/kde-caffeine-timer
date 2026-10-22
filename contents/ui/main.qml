import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PC3
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasma5support as P5

PlasmoidItem {
    id: root
    property bool active: false
    property double expires: 0
    property string mode: "full"
    property int selectedMinutes: 60
    property string controller: ""
    property string errorMessage: ""
    property double nowSeconds: Date.now() / 1000
    property int remaining: expires > 0 ? Math.max(0, Math.ceil(expires - nowSeconds)) : 0
    preferredRepresentation: compactRepresentation
    toolTipMainText: "Caffeine Timer"
    toolTipSubText: active ? (expires === 0 ? "Active indefinitely" : formatTime(remaining) + " remaining") : "Power saving enabled"

    function formatTime(sec) {
        var h = Math.floor(sec / 3600), m = Math.floor((sec % 3600) / 60), s = sec % 60
        return (h > 0 ? String(h).padStart(2, '0') + ':' : '') + String(m).padStart(2, '0') + ':' + String(s).padStart(2, '0')
    }
    function run(args) { executor.connectSource("caffeine-timer-helper " + args) }
    function refresh() { run("status") }
    function start() { run("start " + selectedMinutes + " " + mode) }
    function stop() { run("stop") }
    Component.onCompleted: refresh()
    Timer { interval: 1000; running: root.active; repeat: true; onTriggered: { root.nowSeconds = Date.now() / 1000; if (root.expires > 0 && root.remaining === 0) root.refresh() } }
    Timer { interval: 5000; running: true; repeat: true; onTriggered: root.refresh() }
    P5.DataSource {
        id: executor
        engine: "executable"
        connectedSources: []
        onNewData: function(source, data) {
            disconnectSource(source)
            
            try {
                const result = JSON.parse(data.stdout)
                if (result.error) { root.errorMessage = result.error; return }
                root.active = result.active
                root.expires = result.until || 0
                if (result.mode) root.mode = result.mode
                root.errorMessage = ""
            } catch(e) { root.errorMessage = String(e) }
        }
    }
    compactRepresentation: MouseArea {
        implicitWidth: Kirigami.Units.iconSizes.medium
        implicitHeight: Kirigami.Units.iconSizes.medium
        onClicked: root.expanded = !root.expanded
        Kirigami.Icon {
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height)
            height: width
            source: root.active ? "user-available" : "preferences-system-power-management"
        }
    }
    fullRepresentation: ColumnLayout {
        implicitWidth: Kirigami.Units.gridUnit * 19
        spacing: Kirigami.Units.smallSpacing * 2
        RowLayout {
            Layout.fillWidth: true
            PC3.Label { text: "☕"; font.pixelSize: 26 }
            ColumnLayout {
                PC3.Label { text: "Caffeine Timer"; font.bold: true }
                PC3.Label { text: root.active ? "Keeping your system awake" : "Ready to start"; opacity: 0.7 }
            }
            PC3.Label { text: root.active ? "● ON" : "OFF"; color: root.active ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor }
        }
        PC3.Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.active ? (root.expires === 0 ? "∞" : root.formatTime(root.remaining)) : "--:--"
            font.pixelSize: 36
            font.bold: true
        }
        PC3.Label { Layout.alignment: Qt.AlignHCenter; text: root.active ? "Remaining time" : "Choose a duration"; opacity: 0.65 }
        Flow {
            Layout.fillWidth: true
            spacing: 5
            Repeater {
                model: [{m:15,t:"15m"},{m:30,t:"30m"},{m:60,t:"1h"},{m:120,t:"2h"},{m:240,t:"4h"},{m:0,t:"∞"}]
                delegate: PC3.Button {
                    required property var modelData
                    text: modelData.t
                    checkable: true
                    checked: root.selectedMinutes === modelData.m
                    onClicked: root.selectedMinutes = modelData.m
                }
            }
        }
        PC3.CheckBox {
            text: "Keep display awake too"
            checked: root.mode === "full"
            onToggled: root.mode = checked ? "full" : "sleep"
        }
        RowLayout {
            Layout.fillWidth: true
            PC3.Button { text: "Stop"; enabled: root.active; onClicked: root.stop() }
            PC3.Button { Layout.fillWidth: true; text: root.active ? "Restart timer" : "Start"; highlighted: true; onClicked: root.start() }
        }
        PC3.Label {
            Layout.fillWidth: true
            visible: root.errorMessage.length > 0
            text: root.errorMessage
            wrapMode: Text.Wrap
            color: Kirigami.Theme.negativeTextColor
        }
    }
}
