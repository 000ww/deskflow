/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <X11/Xlib.h>

// X11 defines CursorShape as a macro which conflicts with Qt's enum
#ifdef CursorShape
#undef CursorShape
#endif

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class XWindowsScreen;

/**
 * @brief X11 Xdnd protocol handler for file drag-and-drop
 *
 * Implements the Xdnd (Drag and Drop) protocol for receiving
 * files dropped on the Deskflow window via text/uri-list.
 */
class XWindowsXdndHandler
{
public:
  XWindowsXdndHandler(Display *display, Window window, XWindowsScreen *screen);
  ~XWindowsXdndHandler() = default;

  /**
   * @brief Handle X client message events
   * @param event X client message event
   * @return True if the event was handled
   */
  bool handleClientMessage(const XClientMessageEvent &event);

  /**
   * @brief Handle X selection notification events
   * @param event X selection request event
   * @return True if the event was handled
   */
  bool handleSelectionNotify(const XSelectionEvent &event);

private:
  /**
   * @brief Handle XdndEnter message
   * @param event X client message event
   */
  void handleXdndEnter(const XClientMessageEvent &event);

  /**
   * @brief Handle XdndPosition message
   * @param event X client message event
   */
  void handleXdndPosition(const XClientMessageEvent &event);

  /**
   * @brief Handle XdndLeave message
   * @param event X client message event
   */
  void handleXdndLeave(const XClientMessageEvent &event);

  /**
   * @brief Handle XdndDrop message
   * @param event X client message event
   */
  void handleXdndDrop(const XClientMessageEvent &event);

  /**
   * @brief Send XdndStatus response
   * @param sourceWindow Source window to send to
   * @param accept True to accept the drop
   */
  void sendXdndStatus(Window sourceWindow, bool accept);

  /**
   * @brief Send XdndFinished response
   * @param sourceWindow Source window to send to
   * @param accept True if drop was accepted
   */
  void sendXdndFinished(Window sourceWindow, bool accept);

  /**
   * @brief Request the drop data (text/uri-list)
   */
  void requestDropData();

  /**
   * @brief Process received drop data
   * @param data Raw data from selection
   * @param length Data length
   */
  void processDropData(const unsigned char *data, unsigned long length);

  /**
   * @brief Parse file:// URIs to file paths
   * @param uriList URI list string
   * @return Vector of file paths
   */
  static std::vector<std::filesystem::path> parseUriList(const std::string &uriList);

  Display *m_display;
  Window m_window;
  XWindowsScreen *m_screen;

  // Xdnd atoms
  Atom m_xdndEnter;
  Atom m_xdndPosition;
  Atom m_xdndLeave;
  Atom m_xdndDrop;
  Atom m_xdndStatus;
  Atom m_xdndFinished;
  Atom m_xdndAware;
  Atom m_xdndActionCopy;
  Atom m_uriListAtom;
  Atom m_textPlainAtom;

  // State
  Window m_sourceWindow;
  bool m_dragActive;
  bool m_hasUriList;
  bool m_dropAccepted;
};
