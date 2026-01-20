import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: main
    width: 600
    height: 400
    visible: true
    title: "Qt Logo Window"

    Rectangle {
        anchors.fill: parent
        color: "#202020"

        Image {
            id: logo

            property real vx: 2.5
            property real vy: 2

            source: "qt_logo.svg"
            width: 120
            height: 120

            Timer {
                interval: 2
                running: true
                repeat: true

                onTriggered: {
                    logo.x += logo.vx
                    logo.y += logo.vy

                    if (logo.x <= 0 || logo.x + logo.width >= main.width)
                        logo.vx *= -1

                    if (logo.y <= 0 || logo.y + logo.height >= main.height)
                        logo.vy *= -1
                }
            }
        }
    }
}
