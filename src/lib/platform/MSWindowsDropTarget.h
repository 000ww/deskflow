/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <oleidl.h>

#include <cstdint>
#include <filesystem>
#include <vector>

class MSWindowsScreen;

/**
 * @brief Windows IDropTarget implementation for file drag-and-drop
 *
 * Handles OLE drag-and-drop operations to receive files dropped
 * on the Deskflow window.
 */
class MSWindowsDropTarget : public IDropTarget
{
public:
  MSWindowsDropTarget(MSWindowsScreen *screen);
  ~MSWindowsDropTarget() = default;

  // IUnknown methods
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  // IDropTarget methods
  HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) override;
  HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) override;
  HRESULT STDMETHODCALLTYPE DragLeave() override;
  HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) override;

private:
  /**
   * @brief Extract file paths from IDataObject
   * @param pDataObj OLE data object containing file drop info
   * @param files Output vector of file paths
   * @return True if files were extracted successfully
   */
  static bool extractFiles(IDataObject *pDataObj, std::vector<std::filesystem::path> &files);

  MSWindowsScreen *m_screen;
  LONG m_refCount;
  bool m_acceptDrop;
};
