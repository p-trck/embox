#!/bin/sh

# 버전 정보 (필요시 직접 지정)
VERSION="1.0.0"

# 날짜 (빌드 시점)
DATE=$(date +"%Y-%m-%d %H:%M:%S")

# git commit id (현재 HEAD 기준)
GIT_COMMIT=$(git rev-parse --short HEAD)

# rootfs 경로 (환경에 따라 수정)
ROOTFS_DIR="$1"
if [ -z "$ROOTFS_DIR" ]; then
    echo "Usage: $0 <rootfs_directory>"
    exit 1
fi

# 기존 version 파일이 있다면 삭제
if [ -f "${ROOTFS_DIR}/version" ]; then
    echo "Removing existing version file in ${ROOTFS_DIR}/version"
    rm -f "${ROOTFS_DIR}/version"
fi

# version 파일 생성
echo Update version file in "${ROOTFS_DIR}/version"

echo "Manufacturer: MND" > "${ROOTFS_DIR}/version"
echo "Product: NetSpeaker" >> "${ROOTFS_DIR}/version"

echo "Version: $VERSION" >> "${ROOTFS_DIR}/version"
echo "Date: $DATE" >> "${ROOTFS_DIR}/version"
echo "Git commit: $GIT_COMMIT" >> "${ROOTFS_DIR}/version"

