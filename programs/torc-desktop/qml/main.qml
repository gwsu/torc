import QtQuick 2.1
import QtMultimedia 5.0
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0
import Torc.Core 0.1
import Torc.Media 0.1

ApplicationWindow {
    id: window
    width: 800
    height: 600
    title: qsTr("torc-desktop")

    Component.onCompleted: window.title = TorcLocalContext.GetUuid()

    TabView {
        id: frame
        tabPosition: Qt.TopEdge
        anchors.fill: parent
        anchors.margins: Qt.platform.os === "osx" ? 12 : 2

        Tab {
            title: qsTr("Media")

            SplitView {
                anchors.fill: parent
                anchors.margins: 8
                orientation: Qt.Vertical

                MediaPlayer {
                    id: mediaPlayer
                    source: "/Users/mark/Dropbox/Videos/bourne.mp4"
                    autoPlay: true
                }

                VideoOutput {
                    id: videoOutput
                    height: 400
                    source: mediaPlayer
                }

                Column {
                    TableView {
                        id: mediaView
                        model: TorcMediaMasterFilter {
                            id: mediaFilter
                            sourceModel: TorcMediaMaster
                        }

                        sortIndicatorVisible: true

                        height: parent.height - 34
                        anchors.right: parent.right
                        anchors.left: parent.left

                        onSortIndicatorOrderChanged: mediaFilter.SetSortOrder(mediaView.sortIndicatorOrder, mediaView.sortIndicatorColumn)
                        onSortIndicatorColumnChanged: mediaFilter.SetSortOrder(mediaView.sortIndicatorOrder, mediaView.sortIndicatorColumn)
                        onDoubleClicked: mediaPlayer.source = mediaFilter.GetChildByIndex(currentRow).url

                        TableViewColumn {
                            width: 250
                            title: qsTr("Name")
                            role: "name"
                        }

                        TableViewColumn {
                            width: 350
                            title: qsTr("Location")
                            role: "url"
                        }
                    }

                    Row {
                        spacing: 12
                        Text {
                            text: qsTr("Search")
                        }

                        TextField {
                            text: mediaFilter.textFilter
                            onTextChanged: mediaFilter.SetTextFilter(text)
                        }

                        Button {
                            text: qsTr("Search using")
                            menu: Menu {
                                MenuItem {
                                    text: "Name"
                                    checkable: true
                                    checked: mediaFilter.filterByName
                                    onTriggered: mediaFilter.filterByName = true
                                }
                                MenuItem {
                                    text: "Location"
                                    checkable: true
                                    checked: !mediaFilter.filterByName
                                    onTriggered: mediaFilter.filterByName = false
                                }
                            }
                        }

                        CheckBox{
                            anchors.leftMargin: 16
                            id: videoCheckbox
                            checked: true
                            text: qsTr("Videos")
                            onClicked: mediaFilter.SetMediaTypeFilter(TorcMedia.Video, checked)
                        }
                        CheckBox{
                            id: musicCheckbox
                            checked: true
                            text: qsTr("Music")
                            onClicked: mediaFilter.SetMediaTypeFilter(TorcMedia.Audio, checked)
                        }
                        CheckBox{
                            id: photosCheckbox
                            checked: true
                            text: qsTr("Photos")
                            onClicked: mediaFilter.SetMediaTypeFilter(TorcMedia.Image, checked)
                        }
                    }
                }
            }
        }

        Tab {
            title: qsTr("Status")

            SplitView {
                anchors.fill: parent
                anchors.margins: 8
                orientation: Qt.Vertical

                property var haveDevices: false

                GroupBox {
                    id: noTorcDevices
                    visible: !haveDevices && TorcNetworkedContext
                    anchors.margins: 8
                    Layout.fillWidth: true
                    title: qsTr("No other Torc devices detected on this network")
                }

                GroupBox {
                    id: torcDevices
                    visible: haveDevices && TorcNetworkedContext
                    anchors.margins: 8
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Other Torc devices on this network")

                    TableView {
                        id: torcDevicesView
                        anchors.fill: parent
                        onRowCountChanged: haveDevices = rowCount > 0

                        model: TorcNetworkedContext

                        TableViewColumn {
                            width: 250
                            title: qsTr("Name")
                            role: "m_name"
                        }

                        TableViewColumn {
                            width: 200
                            title: qsTr("Address")
                            role: "m_uiAddress"
                        }

                        TableViewColumn {
                            width: 300
                            title: qsTr("Identifier")
                            role: "m_uuid"
                        }
                    }
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Other interesting stuff")
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Really interesting stuff")
                }
            }
        }

        Tab {
            title: qsTr("Settings")

            SplitView {
                id: settings
                anchors.fill: parent
                anchors.margins: 8
                orientation: Qt.Horizontal

                ScrollView {
                    ListView {
                        id: settingsGroup
                        width: 150

                        delegate: Button {
                            width: parent.width
                            text: modelData.m_uiName
                            enabled: modelData.rowCount() > 0
                        }

                        model: RootSetting
                    }
                }

                ScrollView {
                    id: settingsView
                    Text {
                        text: "Empty"
                    }
                }
            }
        }
    }
}