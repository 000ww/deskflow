/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileReceiver.h"
#include "deskflow/FileSender.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace deskflow {
class IStream;
}

class IEventQueue;

/**
 * @brief File transfer request callback
 * @param transferId Transfer identifier
 * @param sourceScreen Name of the source screen
 * @param fileCount Number of files
 * @param totalSize Total size in bytes
 * @return True if user accepts the transfer
 */
using FileTransferRequestCallback = std::function<bool(
    uint32_t transferId, const std::string &sourceScreen, uint32_t fileCount, uint64_t totalSize
)>;

/**
 * @brief File transfer manager
 *
 * Manages active file transfer sessions between screens.
 * Coordinates FileSender and FileReceiver instances.
 */
class FileTransferManager
{
public:
  /**
   * @brief Get the singleton instance
   */
  static FileTransferManager &instance();

  /**
   * @brief Initialize the manager
   * @param events Event queue
   * @param downloadDir Default download directory
   */
  void init(IEventQueue *events, const std::filesystem::path &downloadDir);

  /**
   * @brief Send files to a remote screen
   * @param files List of file paths to send
   * @param baseDir Base directory for computing relative paths
   * @param stream Output stream to send through
   * @param eventTarget Event target for callbacks
   * @return Transfer ID, or 0 on failure
   */
  uint32_t sendFiles(
      const std::vector<std::filesystem::path> &files, const std::filesystem::path &baseDir,
      deskflow::IStream *stream, void *eventTarget
  );

  /**
   * @brief Handle incoming drag info message
   * @param transferId Transfer identifier
   * @param fileCount Number of files
   * @param totalSize Total size in bytes
   * @param sourceScreen Source screen name
   * @param stream Output stream for response
   * @param eventTarget Event target
   */
  void handleDragInfo(
      uint32_t transferId, uint32_t fileCount, uint64_t totalSize, const std::string &sourceScreen,
      deskflow::IStream *stream, void *eventTarget
  );

  /**
   * @brief Handle incoming file chunk
   * @param transferId Transfer identifier
   * @param mark Chunk type mark
   * @param data Chunk data
   * @return True if handled successfully
   */
  bool handleFileChunk(uint32_t transferId, uint8_t mark, const std::string &data);

  /**
   * @brief Accept an incoming transfer
   * @param transferId Transfer identifier
   * @return True if transfer was accepted
   */
  bool acceptTransfer(uint32_t transferId);

  /**
   * @brief Reject an incoming transfer
   * @param transferId Transfer identifier
   * @param reason Rejection reason
   */
  void rejectTransfer(uint32_t transferId, const std::string &reason = "Rejected by user");

  /**
   * @brief Cancel an active transfer
   * @param transferId Transfer identifier
   * @param reason Cancel reason
   */
  void cancelTransfer(uint32_t transferId, const std::string &reason = "Cancelled by user");

  /**
   * @brief Check if a transfer is active
   * @param transferId Transfer identifier
   * @return True if transfer is active
   */
  bool isTransferActive(uint32_t transferId) const;

  /**
   * @brief Get the next transfer ID
   * @return New unique transfer ID
   */
  uint32_t nextTransferId();

  /**
   * @brief Set the download directory
   * @param dir Download directory path
   */
  void setDownloadDir(const std::filesystem::path &dir);

  /**
   * @brief Get the download directory
   */
  const std::filesystem::path &downloadDir() const
  {
    return m_downloadDir;
  }

  /**
   * @brief Set the file transfer request callback
   *
   * Called when a remote screen wants to send files.
   * The callback should show a UI dialog to the user.
   */
  void setRequestCallback(FileTransferRequestCallback callback)
  {
    m_requestCallback = std::move(callback);
  }

  /**
   * @brief Process pending file transfer events
   *
   * Should be called from the event loop to send file chunks.
   */
  void processEvents();

private:
  FileTransferManager() = default;
  ~FileTransferManager() = default;

  FileTransferManager(const FileTransferManager &) = delete;
  FileTransferManager &operator=(const FileTransferManager &) = delete;

  /// Event queue
  IEventQueue *m_events = nullptr;

  /// Default download directory
  std::filesystem::path m_downloadDir;

  /// Active senders
  std::map<uint32_t, std::unique_ptr<FileSender>> m_senders;

  /// Active receivers
  std::map<uint32_t, std::unique_ptr<FileReceiver>> m_receivers;

  /// Pending transfer requests (waiting for user acceptance)
  struct PendingTransfer
  {
    uint32_t transferId;
    uint32_t fileCount;
    uint64_t totalSize;
    std::string sourceScreen;
    deskflow::IStream *stream;
    void *eventTarget;
  };
  std::map<uint32_t, PendingTransfer> m_pendingTransfers;

  /// Next transfer ID
  uint32_t m_nextTransferId = 1;

  /// File transfer request callback
  FileTransferRequestCallback m_requestCallback;

  /// Mutex for thread safety
  mutable std::mutex m_mutex;
};
