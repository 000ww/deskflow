/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/Chunk.h"
#include "deskflow/ProtocolTypes.h"

#include <cstdint>
#include <string>

constexpr static auto s_fileTransferChunkMetaSize = 7;

namespace deskflow {
class IStream;
}

/**
 * @brief File transfer chunk types
 *
 * Extended chunk types for file transfer protocol.
 */
struct FileChunkType
{
  inline static const auto FileStart = 1;    ///< Start of file (contains relative path)
  inline static const auto FileData = 2;     ///< File content chunk
  inline static const auto FileEnd = 3;      ///< End of current file
  inline static const auto TransferEnd = 4;  ///< End of all files
  inline static const auto TransferCancel = 5; ///< Cancel transfer
};

/**
 * @brief File transfer chunk
 *
 * Represents a single chunk of data in a file transfer operation.
 * Mirrors the ClipboardChunk pattern but for file transfers.
 */
class FileTransferChunk : public Chunk
{
public:
  explicit FileTransferChunk(size_t size);
  ~FileTransferChunk() override = default;

  /**
   * @brief Create a file start chunk
   * @param transferId Unique transfer identifier
   * @param relativePath Relative path of the file
   * @return New chunk allocated on the heap
   */
  static FileTransferChunk *fileStart(uint32_t transferId, const std::string &relativePath);

  /**
   * @brief Create a file data chunk
   * @param transferId Unique transfer identifier
   * @param data File content data
   * @return New chunk allocated on the heap
   */
  static FileTransferChunk *fileData(uint32_t transferId, const std::string &data);

  /**
   * @brief Create a file end chunk
   * @param transferId Unique transfer identifier
   * @return New chunk allocated on the heap
   */
  static FileTransferChunk *fileEnd(uint32_t transferId);

  /**
   * @brief Create a transfer end chunk
   * @param transferId Unique transfer identifier
   * @return New chunk allocated on the heap
   */
  static FileTransferChunk *transferEnd(uint32_t transferId);

  /**
   * @brief Create a transfer cancel chunk
   * @param transferId Unique transfer identifier
   * @param reason Cancel reason message
   * @return New chunk allocated on the heap
   */
  static FileTransferChunk *transferCancel(uint32_t transferId, const std::string &reason);

  /**
   * @brief Assemble a chunk from stream data
   * @param stream Input stream to read from
   * @param transferId Output transfer ID
   * @param mark Output chunk type mark
   * @param data Output chunk data
   * @return Transfer state after assembly
   */
  static TransferState assemble(
      deskflow::IStream *stream, uint32_t &transferId, uint8_t &mark, std::string &data
  );

  /**
   * @brief Send a chunk to a stream
   * @param stream Output stream to write to
   * @param data Chunk data to send
   */
  static void send(deskflow::IStream *stream, void *data);

private:
  static size_t s_expectedSize;
};
