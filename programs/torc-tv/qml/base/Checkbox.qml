import QtQuick 2.0

FocusScope {
    id: checkbox
    signal clicked (var event)
    Keys.onPressed: {
        if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
            keypressed = true;
            clicked(undefined);
        }
    }
    Keys.onReleased: {
        if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return)
            keypressed = false;
    }
    property alias mouseArea: mousearea
    property bool enabled: false
    property bool pressed: mousearea.pressed || keypressed
    property bool hovered: mousearea.containsMouse
    property bool keypressed: false

    onClicked: enabled = !enabled;

    MouseArea {
        id: mousearea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: checkbox.clicked(mouse);
    }
}
