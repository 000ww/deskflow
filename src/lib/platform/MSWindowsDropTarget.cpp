/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsDropTarget.h"

#include "base/Log.h"
#include "deskflow/FileTransferManager.h"
#include "platform/MSWindowsScreen.h"

#include <Shellapi.h>

MSWindowsDropTarget::MSWindowsDropTarget(MSWindowsScreen *screen)
    : m_screen(screen),
      m_refCount(1),
      m_acceptDrop(false)
{
}

HRESULT STDMETHODCALLTYPE MSWindowsDropTarget::QueryInterface(REFIID riid, void **ppvObject)
{
  if (!ppvObject) {
    return E_INVALIDARG;
  }

  if (riid == IID_IUnknown || riid == IID_IDropTarget) {
    *ppvObject = static_cast<IDropTarget *>(this);
    AddRef();
    return S_OK;
  }

  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MSWindowsDropTarget::AddRef()
{
  return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE MSWindowsDropTarget::Release()
{
  LONG count = InterlockedDecrement(&m_refCount);
  if (count == 0) {
    delete this;
  }
  return count;
}

HRESULT STDMETHODCALLTYPE
MSWindowsDropTarget::DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
  if (!pDataObj || !pdwEffect) {
    return E_INVALIDARG;
  }

  // Check if the data object contains files
  std::vector<std::filesystem::path> files;
  m_acceptDrop = extractFiles(pDataObj, files);

  if (m_acceptDrop) {
    *pdwEffect = DROPEFFECT_COPY;
    LOG_DEBUG("Windows drag-and-drop: %zu files entering", files.size());
  } else {
    *pdwEffect = DROPEFFECT_NONE;
  }

  return S_OK;
}

HRESULT STDMETHODCALLTYPE MSWindowsDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
  if (!pdwEffect) {
    return E_INVALIDARG;
  }

  if (m_acceptDrop) {
    *pdwEffect = DROPEFFECT_COPY;
  } else {
    *pdwEffect = DROPEFFECT_NONE;
  }

  return S_OK;
}

HRESULT STDMETHODCALLTYPE MSWindowsDropTarget::DragLeave()
{
  m_acceptDrop = false;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MSWindowsDropTarget::Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
  if (!pDataObj || !pdwEffect) {
    return E_INVALIDARG;
  }

  *pdwEffect = DROPEFFECT_NONE;

  if (!m_acceptDrop) {
    return S_OK;
  }

  // Extract file paths
  std::vector<std::filesystem::path> files;
  if (!extractFiles(pDataObj, files) || files.empty()) {
    LOG_WARN("Windows drag-and-drop: failed to extract files");
    return S_OK;
  }

  LOG_DEBUG("Windows drag-and-drop: %zu files dropped", files.size());

  // Determine base directory
  std::filesystem::path baseDir;
  if (!files.empty()) {
    baseDir = files[0].parent_path();
  }

  // Send files through FileTransferManager
  FileTransferManager::instance().sendFiles(files, baseDir, nullptr, m_screen);

  m_acceptDrop = false;
  return S_OK;
}

bool MSWindowsDropTarget::extractFiles(IDataObject *pDataObj, std::vector<std::filesystem::path> &files)
{
  FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  STGMEDIUM stg = {TYMED_HGLOBAL, nullptr, nullptr};

  if (FAILED(pDataObj->GetData(&fmt, &stg))) {
    return false;
  }

  HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
  if (!hDrop) {
    ReleaseStgMedium(&stg);
    return false;
  }

  UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
  for (UINT i = 0; i < fileCount; i++) {
    WCHAR filePath[MAX_PATH];
    if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
      files.emplace_back(filePath);
    }
  }

  GlobalUnlock(stg.hGlobal);
  ReleaseStgMedium(&stg);

  return !files.empty();
}
