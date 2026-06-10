/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/XWindowsScreen.h"

#include "base/Log.h"
#include "deskflow/FileTransferManager.h"

#ifdef CursorShape
#undef CursorShape
#endif
#ifdef Status
#undef Status
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef True
#undef True
#endif
#ifdef False
#undef False
#endif

#include "platform/XWindowsXdndHandler.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <cstdio>
#include <sstream>

XWindowsXdndHandler::XWindowsXdndHandler(Display *display, Window window, XWindowsScreen *screen)
    : m_display(display),
      m_window(window),
      m_screen(screen),
      m_sourceWindow(None),
      m_dragActive(false),
      m_hasUriList(false),
      m_dropAccepted(false)
{
  m_xdndEnter = XInternAtom(m_display, "XdndEnter", False);
  m_xdndPosition = XInternAtom(m_display, "XdndPosition", False);
  m_xdndLeave = XInternAtom(m_display, "XdndLeave", False);
  m_xdndDrop = XInternAtom(m_display, "XdndDrop", False);
  m_xdndStatus = XInternAtom(m_display, "XdndStatus", False);
  m_xdndFinished = XInternAtom(m_display, "XdndFinished", False);
  m_xdndAware = XInternAtom(m_display, "XdndAware", False);
  m_xdndActionCopy = XInternAtom(m_display, "XdndActionCopy", False);
  m_uriListAtom = XInternAtom(m_display, "text/uri-list", False);
  m_textPlainAtom = XInternAtom(m_display, "text/plain", False);

  // Register window as Xdnd-aware
  Atom version = 5;
  XChangeProperty(m_display, m_window, m_xdndAware, XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char *>(&version), 1);

  fprintf(stderr, "X11: Xdnd handler initialized for window %lu\n", m_window);
}

bool XWindowsXdndHandler::handleClientMessage(const XClientMessageEvent &event)
{
  if (event.message_type == m_xdndEnter) {
    handleXdndEnter(event);
    return true;
  } else if (event.message_type == m_xdndPosition) {
    handleXdndPosition(event);
    return true;
  } else if (event.message_type == m_xdndLeave) {
    handleXdndLeave(event);
    return true;
  } else if (event.message_type == m_xdndDrop) {
    handleXdndDrop(event);
    return true;
  }
  return false;
}

bool XWindowsXdndHandler::handleSelectionNotify(const XSelectionEvent &event)
{
  if (event.selection == XInternAtom(m_display, "XdndSelection", False) && event.property == m_uriListAtom) {
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long bytesAfter;
    unsigned char *data = nullptr;

    if (XGetWindowProperty(
            m_display, m_window, m_uriListAtom, 0, 65536, True, AnyPropertyType, &actualType, &actualFormat,
            &itemCount, &bytesAfter, &data
        ) == Success &&
        data) {
      processDropData(data, itemCount);
      XFree(data);
    }
    return true;
  }
  return false;
}

void XWindowsXdndHandler::handleXdndEnter(const XClientMessageEvent &event)
{
  m_sourceWindow = event.data.l[0];
  m_dragActive = true;
  m_hasUriList = false;

  unsigned long flags = event.data.l[1];
  bool hasThreeTypes = (flags & 1) != 0;

  Atom types[3];
  types[0] = event.data.l[2];
  types[1] = event.data.l[3];
  types[2] = event.data.l[4];

  for (int i = 0; i < 3; i++) {
    if (types[i] == m_uriListAtom) {
      m_hasUriList = true;
      break;
    }
  }

  if (!m_hasUriList && hasThreeTypes) {
    Atom actualType;
    int actualFormat;
    unsigned long itemCount;
    unsigned long bytesAfter;
    unsigned char *data = nullptr;

    if (XGetWindowProperty(
            m_display, m_sourceWindow, XInternAtom(m_display, "XdndTypeList", False), 0, 65536, False, XA_ATOM,
            &actualType, &actualFormat, &itemCount, &bytesAfter, &data
        ) == Success &&
        data) {
      Atom *typeList = reinterpret_cast<Atom *>(data);
      for (unsigned long i = 0; i < itemCount; i++) {
        if (typeList[i] == m_uriListAtom) {
          m_hasUriList = true;
          break;
        }
      }
      XFree(data);
    }
  }

  fprintf(stderr, "X11: XdndEnter from window %lu, hasUriList=%d\n", m_sourceWindow, m_hasUriList);
}

void XWindowsXdndHandler::handleXdndPosition(const XClientMessageEvent &event)
{
  bool accept = m_hasUriList;
  sendXdndStatus(m_sourceWindow, accept);
}

void XWindowsXdndHandler::handleXdndLeave(const XClientMessageEvent &event)
{
  m_dragActive = false;
  m_hasUriList = false;
  m_sourceWindow = None;
  fprintf(stderr, "X11: XdndLeave\n");
}

void XWindowsXdndHandler::handleXdndDrop(const XClientMessageEvent &event)
{
  if (!m_hasUriList) {
    sendXdndFinished(m_sourceWindow, false);
    return;
  }
  requestDropData();
}

void XWindowsXdndHandler::sendXdndStatus(Window sourceWindow, bool accept)
{
  XClientMessageEvent status;
  status.type = ClientMessage;
  status.serial = 0;
  status.send_event = True;
  status.display = m_display;
  status.window = sourceWindow;
  status.message_type = m_xdndStatus;
  status.format = 32;
  status.data.l[0] = m_window;
  status.data.l[1] = (accept ? 1 : 0) | 2;
  status.data.l[2] = 0;
  status.data.l[3] = 0;
  status.data.l[4] = accept ? m_xdndActionCopy : None;

  XSendEvent(m_display, sourceWindow, False, NoEventMask, reinterpret_cast<XEvent *>(&status));
  XFlush(m_display);
}

void XWindowsXdndHandler::sendXdndFinished(Window sourceWindow, bool accept)
{
  XClientMessageEvent finished;
  finished.type = ClientMessage;
  finished.serial = 0;
  finished.send_event = True;
  finished.display = m_display;
  finished.window = sourceWindow;
  finished.message_type = m_xdndFinished;
  finished.format = 32;
  finished.data.l[0] = m_window;
  finished.data.l[1] = accept ? 1 : 0;
  finished.data.l[2] = accept ? m_xdndActionCopy : None;

  XSendEvent(m_display, sourceWindow, False, NoEventMask, reinterpret_cast<XEvent *>(&finished));
  XFlush(m_display);

  m_dragActive = false;
  m_hasUriList = false;
  m_sourceWindow = None;
}

void XWindowsXdndHandler::requestDropData()
{
  XConvertSelection(m_display, XInternAtom(m_display, "XdndSelection", False), m_uriListAtom, m_uriListAtom, m_window, CurrentTime);
  XFlush(m_display);
}

void XWindowsXdndHandler::processDropData(const unsigned char *data, unsigned long length)
{
  std::string uriList(reinterpret_cast<const char *>(data), length);
  std::vector<std::filesystem::path> files = parseUriList(uriList);

  if (files.empty()) {
    fprintf(stderr, "X11: XdndDrop: no valid files found\n");
    sendXdndFinished(m_sourceWindow, false);
    return;
  }

  fprintf(stderr, "X11: XdndDrop: %zu files dropped\n", files.size());

  // Determine base directory
  std::filesystem::path baseDir;
  if (!files.empty()) {
    baseDir = files[0].parent_path();
  }

  // Call file transfer through screen's event system
  // The actual FileTransferManager call will be wired through the screen
  if (m_screen) {
    m_screen->handleFileDrop(files, baseDir);
  }

  sendXdndFinished(m_sourceWindow, true);
}

std::vector<std::filesystem::path> XWindowsXdndHandler::parseUriList(const std::string &uriList)
{
  std::vector<std::filesystem::path> files;
  std::istringstream stream(uriList);
  std::string line;

  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (line.empty() || line[0] == '#') {
      continue;
    }

    if (line.substr(0, 7) == "file://") {
      std::string path = line.substr(7);

      std::string decoded;
      for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == '%' && i + 2 < path.size()) {
          int value;
          std::istringstream hex(path.substr(i + 1, 2));
          if (hex >> std::hex >> value) {
            decoded += static_cast<char>(value);
            i += 2;
          } else {
            decoded += path[i];
          }
        } else if (path[i] == '+') {
          decoded += ' ';
        } else {
          decoded += path[i];
        }
      }

      files.emplace_back(decoded);
    }
  }

  return files;
}
