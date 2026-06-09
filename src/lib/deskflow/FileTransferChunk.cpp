/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferChunk.h"

#include "base/Log.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"

#include <cstring>

size_t FileTransferChunk::s_expectedSize = 0;

FileTransferChunk::FileTransferChunk(size_t size) : Chunk(size)
{
  m_dataSize = size - s_fileTransferChunkMetaSize;
}

FileTransferChunk *FileTransferChunk::fileStart(uint32_t transferId, const std::string &relativePath)
{
  size_t pathLength = relativePath.size();
  auto *chunk = new FileTransferChunk(pathLength + s_fileTransferChunkMetaSize);
  char *data = chunk->m_chunk;

  std::memcpy(data, &transferId, 4);
  data[4] = FileChunkType::FileStart;
  std::memcpy(&data[5], relativePath.c_str(), pathLength);
  data[pathLength + s_fileTransferChunkMetaSize - 1] = '\0';

  return chunk;
}

FileTransferChunk *FileTransferChunk::fileData(uint32_t transferId, const std::string &data)
{
  size_t dataSize = data.size();
  auto *chunk = new FileTransferChunk(dataSize + s_fileTransferChunkMetaSize);
  char *chunkData = chunk->m_chunk;

  std::memcpy(chunkData, &transferId, 4);
  chunkData[4] = FileChunkType::FileData;
  std::memcpy(&chunkData[5], data.c_str(), dataSize);
  chunkData[dataSize + s_fileTransferChunkMetaSize - 1] = '\0';

  return chunk;
}

FileTransferChunk *FileTransferChunk::fileEnd(uint32_t transferId)
{
  auto *chunk = new FileTransferChunk(s_fileTransferChunkMetaSize);
  char *data = chunk->m_chunk;

  std::memcpy(data, &transferId, 4);
  data[4] = FileChunkType::FileEnd;
  data[s_fileTransferChunkMetaSize - 1] = '\0';

  return chunk;
}

FileTransferChunk *FileTransferChunk::transferEnd(uint32_t transferId)
{
  auto *chunk = new FileTransferChunk(s_fileTransferChunkMetaSize);
  char *data = chunk->m_chunk;

  std::memcpy(data, &transferId, 4);
  data[4] = FileChunkType::TransferEnd;
  data[s_fileTransferChunkMetaSize - 1] = '\0';

  return chunk;
}

FileTransferChunk *FileTransferChunk::transferCancel(uint32_t transferId, const std::string &reason)
{
  size_t reasonLength = reason.size();
  auto *chunk = new FileTransferChunk(reasonLength + s_fileTransferChunkMetaSize);
  char *data = chunk->m_chunk;

  std::memcpy(data, &transferId, 4);
  data[4] = FileChunkType::TransferCancel;
  std::memcpy(&data[5], reason.c_str(), reasonLength);
  data[reasonLength + s_fileTransferChunkMetaSize - 1] = '\0';

  return chunk;
}

TransferState FileTransferChunk::assemble(
    deskflow::IStream *stream, uint32_t &transferId, uint8_t &mark, std::string &data
)
{
  using enum TransferState;

  if (!ProtocolUtil::readf(stream, kMsgDFileTransfer + 4, &transferId, &mark, &data)) {
    return Error;
  }

  if (mark == FileChunkType::FileStart) {
    LOG_DEBUG("file transfer: start receiving file '%s'", data.c_str());
    return Started;
  } else if (mark == FileChunkType::FileData) {
    return InProgress;
  } else if (mark == FileChunkType::FileEnd) {
    LOG_DEBUG("file transfer: file end");
    return Finished;
  } else if (mark == FileChunkType::TransferEnd) {
    LOG_DEBUG("file transfer: all files transferred");
    return Finished;
  } else if (mark == FileChunkType::TransferCancel) {
    LOG_WARN("file transfer: cancelled: %s", data.c_str());
    return Error;
  }

  LOG_ERR("file transfer: unknown chunk type %d", mark);
  return Error;
}

void FileTransferChunk::send(deskflow::IStream *stream, void *data)
{
  const auto *chunk = static_cast<FileTransferChunk *>(data);

  LOG_VERBOSE("sending file transfer chunk");

  const char *chunkData = chunk->m_chunk;
  uint32_t transferId;
  std::memcpy(&transferId, chunkData, 4);
  uint8_t mark = chunkData[4];
  std::string payload(&chunkData[5], chunk->m_dataSize);

  switch (mark) {
  case FileChunkType::FileStart:
    LOG_VERBOSE("sending file start: path=%s", payload.c_str());
    break;

  case FileChunkType::FileData:
    LOG_VERBOSE("sending file data: size=%zu", payload.size());
    break;

  case FileChunkType::FileEnd:
    LOG_VERBOSE("sending file end");
    break;

  case FileChunkType::TransferEnd:
    LOG_VERBOSE("sending transfer end");
    break;

  case FileChunkType::TransferCancel:
    LOG_VERBOSE("sending transfer cancel: reason=%s", payload.c_str());
    break;

  default:
    break;
  }

  ProtocolUtil::writef(stream, kMsgDFileTransfer, transferId, mark, &payload);
}
