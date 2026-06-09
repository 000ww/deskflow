/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TransferProgressWidget.h"
#include "ui_TransferProgressWidget.h"

#include <QDateTime>

TransferProgressWidget::TransferProgressWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::TransferProgressWidget)
{
  ui->setupUi(this);
  m_lastTime = QDateTime::currentMSecsSinceEpoch();

  connect(ui->buttonCancel, &QPushButton::clicked, this, &TransferProgressWidget::onCancelClicked);
}

TransferProgressWidget::~TransferProgressWidget()
{
  delete ui;
}

void TransferProgressWidget::updateProgress(
    uint64_t bytesTransferred, uint64_t totalBytes, uint32_t filesCompleted, uint32_t totalFiles,
    const std::string &currentFile
)
{
  // Calculate speed
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  qint64 elapsed = now - m_lastTime;
  if (elapsed > 0) {
    uint64_t bytesDelta = bytesTransferred - m_lastBytesTransferred;
    uint64_t bytesPerSecond = (bytesDelta * 1000) / static_cast<uint64_t>(elapsed);
    ui->labelSpeed->setText(formatSpeed(bytesPerSecond));
  }
  m_lastBytesTransferred = bytesTransferred;
  m_lastTime = now;

  // Update file name
  ui->labelCurrentFile->setText(QString::fromStdString(currentFile));

  // Update progress bar
  if (totalBytes > 0) {
    int percent = static_cast<int>((bytesTransferred * 100) / totalBytes);
    ui->progressBar->setValue(percent);
    ui->labelPercent->setText(QString("%1%").arg(percent));
  } else {
    ui->progressBar->setValue(0);
    ui->labelPercent->setText("");
  }

  // Update file count
  ui->labelFileProgress->setText(tr("%1 / %2 files").arg(filesCompleted).arg(totalFiles));

  // Update bytes
  ui->labelBytes->setText(tr("%1 / %2").arg(formatBytes(bytesTransferred)).arg(formatBytes(totalBytes)));
}

void TransferProgressWidget::setComplete(bool success, const std::string &message)
{
  ui->buttonCancel->setEnabled(false);
  ui->progressBar->setValue(100);
  ui->labelSpeed->setText("");

  if (success) {
    ui->labelStatus->setText(tr("Transfer complete"));
    ui->labelStatus->setStyleSheet("color: green;");
  } else {
    ui->labelStatus->setText(QString::fromStdString(message));
    ui->labelStatus->setStyleSheet("color: red;");
  }
}

void TransferProgressWidget::onCancelClicked()
{
  emit cancelRequested(m_transferId);
}

QString TransferProgressWidget::formatBytes(uint64_t bytes)
{
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unitIndex = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024.0 && unitIndex < 4) {
    size /= 1024.0;
    unitIndex++;
  }

  if (unitIndex == 0) {
    return QString("%1 %2").arg(bytes).arg(units[unitIndex]);
  } else {
    return QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unitIndex]);
  }
}

QString TransferProgressWidget::formatSpeed(uint64_t bytesPerSecond)
{
  return QString("%1/s").arg(formatBytes(bytesPerSecond));
}
