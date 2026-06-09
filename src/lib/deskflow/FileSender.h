/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <queue>
#include <string>
#include <vector>

namespace deskflow {
class IStream;
}

class IEventQueue;

/**
 * @brief File information for transfer
 */
struct FileTransferInfo
{
  std::filesystem::path absolutePath;  ///< Absolute path on sender
  std::string relativePath;            ///< Relative path for receiver
  uint64_t size = 0;                   ///< File size in bytes
  bool isDirectory = false;            ///< True if this is a directory
};

/**
 * @brief Progress callback
 * @param bytesTransferred Bytes transferred so far for current file
 * @param totalBytes Total bytes for current file
 * @param filesTransferred Number of files completed
 * @param totalFiles Total number of files
 */
using FileTransferProgressCallback = std::function<void(
    uint64_t bytesTransferred, uint64_t totalBytes, uint32_t filesTransferred, uint32_t totalFiles
)>;

/**
 * @brief Completion callback
 * @param success True if all files transferred successfully
 * @param message Error message if failed
 */
using FileTransferCompleteCallback = std::function<void(bool success, const std::string &message)>;

/**
 * @brief File sender
 *
 * Handles reading files from disk and sending them in chunks
 * using the file transfer protocol.
 */
class FileSender
{
public:
  FileSender(uint32_t transferId, IEventQueue *events, void *eventTarget);
  ~FileSender() = default;

  /**
   * @brief Queue files for transfer
   * @param paths List of absolute paths to transfer
   * @param baseDir Base directory for computing relative paths
   */
  void queueFiles(const std::vector<std::filesystem::path> &paths, const std::filesystem::path &baseDir);

  /**
   * @brief Start sending files
   *
   * Sends the drag info message and begins file transfer.
   * Must be called after queueFiles().
   */
  void start();

  /**
   * @brief Send the next chunk of the current file
   *
   * Called by the event loop to send one chunk at a time.
   * @return True if there are more chunks to send
   */
  bool sendNextChunk();

  /**
   * @brief Cancel the transfer
   * @param reason Cancel reason
   */
  void cancel(const std::string &reason);

  /**
   * @brief Check if transfer is complete
   */
  bool isComplete() const
  {
    return m_complete;
  }

  /**
   * @brief Get transfer ID
   */
  uint32_t transferId() const
  {
    return m_transferId;
  }

  /**
   * @brief Set progress callback
   */
  void setProgressCallback(FileTransferProgressCallback callback)
  {
    m_progressCallback = std::move(callback);
  }

  /**
   * @brief Set completion callback
   */
  void setCompleteCallback(FileTransferCompleteCallback callback)
  {
    m_completeCallback = std::move(callback);
  }

  /**
   * @brief Get total bytes to transfer
   */
  uint64_t totalBytes() const
  {
    return m_totalBytes;
  }

  /**
   * @brief Get total files to transfer
   */
  uint32_t totalFiles() const
  {
    return m_totalFiles;
  }

  /**
   * @brief Get total bytes sent so far
   */
  uint64_t bytesSent() const
  {
    return m_bytesSent;
  }

  /**
   * @brief Get number of files completed
   */
  uint32_t filesCompleted() const
  {
    return m_filesCompleted;
  }

private:
  /**
   * @brief Collect all files recursively from a directory
   */
  void collectFiles(const std::filesystem::path &path, const std::filesystem::path &baseDir);

  /**
   * @brief Open the next file for reading
   * @return True if a file was opened
   */
  bool openNextFile();

  /**
   * @brief Send the drag info message
   */
  void sendDragInfo();

  /// Transfer ID
  uint32_t m_transferId;

  /// Event queue for sending events
  IEventQueue *m_events;

  /// Event target for callbacks
  void *m_eventTarget;

  /// Queue of files to transfer
  std::queue<FileTransferInfo> m_fileQueue;

  /// Total bytes to transfer
  uint64_t m_totalBytes = 0;

  /// Total files to transfer
  uint32_t m_totalFiles = 0;

  /// Bytes sent so far
  uint64_t m_bytesSent = 0;

  /// Files completed so far
  uint32_t m_filesCompleted = 0;

  /// Current file being sent
  std::ifstream m_currentFile;

  /// Current file info
  FileTransferInfo m_currentFileInfo;

  /// Bytes sent for current file
  uint64_t m_currentFileBytesSent = 0;

  /// Whether transfer is complete
  bool m_complete = false;

  /// Whether transfer has started
  bool m_started = false;

  /// Progress callback
  FileTransferProgressCallback m_progressCallback;

  /// Completion callback
  FileTransferCompleteCallback m_completeCallback;

  /// Chunk size (512 KB)
  static constexpr size_t kChunkSize = 512 * 1024;
};
