import QtQuick 2.0
import QtGraphicalEffects 1.0
import "base"

Checkbox {
    id: checkbox
    property color basecolor: '#d9d9d9'

    width: 45
    height: 45

    Rectangle {
        id: image
        anchors.fill: parent
        radius: 5
        gradient: Gradient {
            GradientStop { position: 0; color: 'white' }
            GradientStop { position: 0.5; color: basecolor }
        }
        border.color: 'lightgray'
        border.width: 1
        visible: false
    }

    Dropshadow {
        source: image
        color: (hovered | activeFocus) ? "#aa000000" : dropcolor
        verticalOffset: (hovered | activeFocus) ? 6 : 3
    }

    Text {
        id: check
        anchors.centerIn: parent
        color: '#111111'
        font.pointSize: 35
        font.family: webfont.name
        text: checkbox.enabled ? "\uf05d" : "\uf10c"
        Behavior on text { PropertyAnimation {} }
    }
}
