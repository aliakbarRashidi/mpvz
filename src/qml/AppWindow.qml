/*
 * This file is a copy of ApplicationWindow.qml copied from:
 * http://ftp.vim.org/languages/qt/ministro/android/qt5/objects/5.32-armeabi/qml/QtQuick/Controls/ApplicationWindow.qml
 *
 * The following changes are made at the bottom of this file:
 *     - Adding property bool menuBarVisible: false
 *     - Adding a "noMenuBar" state to "fix" hiding the menubar.
 */

/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick.Window 2.1
import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.0
import QtQuick.Controls.Private 1.0

/*!
	\qmltype ApplicationWindow
	\since 5.1
	\inqmlmodule QtQuick.Controls
	\ingroup applicationwindow
	\brief Provides a top-level application window.

	\image applicationwindow.png

	ApplicationWindow is a \l Window that adds convenience for positioning items,
	such as \l MenuBar, \l ToolBar, and \l StatusBar in a platform independent
	manner.

	\code
	ApplicationWindow {
		id: window
		visible: true

		menuBar: MenuBar {
			Menu { MenuItem {...} }
			Menu { MenuItem {...} }
		}

		toolBar: ToolBar {
			RowLayout {
				anchors.fill: parent
				ToolButton {...}
			}
		}

		TabView {
			id: myContent
			anchors.fill: parent
			...
		}
	}
	\endcode

	\note By default, an ApplicationWindow is not visible.

	The \l{Qt Quick Controls - Gallery} example is a good starting
	point to explore this type.
*/

Window {
	id: root

	/*!
		\qmlproperty MenuBar ApplicationWindow::menuBar

		This property holds the \l MenuBar.

		By default, this value is not set.
	*/
	property MenuBar menuBar: null

	/*!
		\qmlproperty Item ApplicationWindow::toolBar

		This property holds the toolbar \l Item.

		It can be set to any Item type, but is generally used with \l ToolBar.

		By default, this value is not set. When you set the toolbar item, it will
		be anchored automatically into the application window.
	*/
	property Item toolBar

	/*!
		\qmlproperty Item ApplicationWindow::statusBar

		This property holds the status bar \l Item.

		It can be set to any Item type, but is generally used with \l StatusBar.

		By default, this value is not set. When you set the status bar item, it
		will be anchored automatically into the application window.
	*/
	property Item statusBar

	// The below documentation was supposed to be written as a grouped property, but qdoc would
	// not render it correctly due to a bug (https://bugreports.qt-project.org/browse/QTBUG-34206)
	/*!
		\qmlproperty ContentItem ApplicationWindow::contentItem

		This group holds the size constraints of the content item. This is the area between the
		\l ToolBar and the \l StatusBar.
		The \l ApplicationWindow will use this as input when calculating the effective size
		constraints of the actual window.
		It holds these 6 properties for describing the minimum, implicit and maximum sizes:
		\table
			\header \li Grouped property            \li Description
			\row    \li contentItem.minimumWidth    \li The minimum width of the content item.
			\row    \li contentItem.minimumHeight   \li The minimum height of the content item.
			\row    \li contentItem.implicitWidth   \li The implicit width of the content item.
			\row    \li contentItem.implicitHeight  \li The implicit height of the content item.
			\row    \li contentItem.maximumWidth    \li The maximum width of the content item.
			\row    \li contentItem.maximumHeight   \li The maximum height of the content item.
		\endtable
	*/
	property alias contentItem : contentArea

	/*! \internal */
	property real __topBottomMargins: contentArea.y + statusBarArea.height
	/*! \internal
		There is a similar macro QWINDOWSIZE_MAX in qwindow_p.h that is used to limit the
		range of QWindow::maximum{Width,Height}
		However, in case we have a very big number (> 2^31) conversion will fail, and it will be
		converted to 0, resulting in that we will call setMaximumWidth(0)....
		We therefore need to enforce the limit at a level where we are still operating on
		floating point values.
	*/
	readonly property real __qwindowsize_max: (1 << 24) - 1

	/*! \internal */
	property real __width: 0
	Binding {
		target: root
		property: "__width"
		when: root.minimumWidth <= root.maximumWidth
		value: Math.max(Math.min(root.maximumWidth, contentArea.implicitWidth), root.minimumWidth)
	}
	/*! \internal */
	property real __height: 0
	Binding {
		target: root
		property: "__height"
		when: root.minimumHeight <= root.maximumHeight
		value: Math.max(Math.min(root.maximumHeight, contentArea.implicitHeight), root.minimumHeight)
	}
	width: contentArea.__noImplicitWidthGiven ? 0 : __width
	height: contentArea.__noImplicitHeightGiven ? 0 : __height

	minimumWidth: contentArea.__noMinimumWidthGiven ? 0 : contentArea.minimumWidth
	minimumHeight: contentArea.__noMinimumHeightGiven ? 0 : (contentArea.minimumHeight + __topBottomMargins)

	maximumWidth: Math.min(__qwindowsize_max, contentArea.maximumWidth)
	maximumHeight: Math.min(__qwindowsize_max, contentArea.maximumHeight + __topBottomMargins)
	onToolBarChanged: { if (toolBar) { toolBar.parent = toolBarArea } }

	onStatusBarChanged: { if (statusBar) { statusBar.parent = statusBarArea } }

	onVisibleChanged: { if (visible && menuBar) { menuBar.__parentWindow = root } }

	/*! \internal */
	default property alias data: contentArea.data

	color: SystemPaletteSingleton.window(true)

	flags: Qt.Window | Qt.WindowFullscreenButtonHint |
		Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinMaxButtonsHint |
		Qt.WindowCloseButtonHint | Qt.WindowFullscreenButtonHint
	// QTBUG-35049: Windows is removing features we didn't ask for, even though Qt::CustomizeWindowHint is not set
	// Otherwise Qt.Window | Qt.WindowFullscreenButtonHint would be enough

	Item {
		id: backgroundItem
		anchors.fill: parent

		Keys.forwardTo: menuBar ? [menuBar.__contentItem] : []

		ContentItem {
			id: contentArea
			anchors.top: toolBarArea.bottom
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.bottom: statusBarArea.top
		}

		Item {
			id: toolBarArea
			anchors.top: parent.top
			anchors.left: parent.left
			anchors.right: parent.right
			implicitHeight: childrenRect.height
			height: visibleChildren.length > 0 ? implicitHeight: 0
		}

		Item {
			id: statusBarArea
			anchors.bottom: parent.bottom
			anchors.left: parent.left
			anchors.right: parent.right
			implicitHeight: childrenRect.height
			height: visibleChildren.length > 0 ? implicitHeight: 0
		}

		onVisibleChanged: if (visible && menuBar) menuBar.__parentWindow = root

		states: [
			State {
				name: "hasMenuBar"
				when: menuBar && !menuBar.__isNative && menuBarVisible

				ParentChange {
					target: menuBar.__contentItem
					parent: backgroundItem
				}

				PropertyChanges {
					target: menuBar.__contentItem
					x: 0
					y: 0
					width: backgroundItem.width
				}

				AnchorChanges {
					target: toolBarArea
					anchors.top: menuBar.__contentItem.bottom
				}
			},
			State {
				name: "noMenuBar"
				when: menuBar && !menuBar.__isNative && !menuBarVisible

				ParentChange {
					target: menuBar.__contentItem
					parent: null
				}

				PropertyChanges {
					target: menuBar.__contentItem
					x: 0
					y: 0
					width: 0
					height: 0
				}

				AnchorChanges {
					target: toolBarArea
					anchors.top: root.top
				}
			}
		]
	}

	property bool menuBarVisible: menuBar || false
}
