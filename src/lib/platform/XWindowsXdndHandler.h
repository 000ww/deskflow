/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <X11/Xlib.h>

#ifdef CursorShape
#undef CursorShape
#endif

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class XWindowsScreen;

class XWindowsXdndHandler
{
public:
  XWindowsXdndHandler(Display *display, Window window, XWindowsScreen *screen);
  ~XWindowsXdndHandler() = default;

  bool handleClientMessage(const XClientMessageEvent &event);
  bool handleSelectionNotify(const XSelectionEvent &event);

private:
  void handleXdndEnter(const XClientMessageEvent &event);
  void handleXdndPosition(const XClientMessageEvent &event);
  void handleXdndLeave(const XClientMessageEvent &event);
  void handleXdndDrop(const XClientMessageEvent &event);
  void sendXdndStatus(Window sourceWindow, bool accept);
  void sendXdndFinished(Window sourceWindow, bool accept);
  void requestDropData();
  void processDropData(const unsigned char *data, unsigned long length);
  static std::vector<std::filesystem::path> parseUriList(const std::string &uriList);

  Display *m_display;
  Window m_window;
  XWindowsScreen *m_screen;

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

  Window m_sourceWindow;
  bool m_dragActive;
  bool m_hasUriList;
  bool m_dropAccepted;
};
