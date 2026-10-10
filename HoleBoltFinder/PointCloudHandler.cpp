#include "pch.h"
#include "PointCloudHandler.h"
#include <QFile>
#include <limits>

// Minimal LZF block decompressor (liblzf-compatible).
// PCD binary_compressed uses raw LZF blocks.
static int lzfDecompress(const char* in, int inLen, char* out, int outLen)
{
    const unsigned char* ip     = reinterpret_cast<const unsigned char*>(in);
    const unsigned char* ip_end = ip + inLen;
    unsigned char*       op     = reinterpret_cast<unsigned char*>(out);
    unsigned char*       op_end = op + outLen;

    while (ip < ip_end)
    {
        unsigned int ctrl = *ip++;

        if (ctrl < 32)   // literal run: ctrl+1 bytes
        {
            unsigned int len = ctrl + 1;
            if (op + len > op_end || ip + len > ip_end) return -1;
            memcpy(op, ip, len);
            op += len;
            ip += len;
        }
        else             // back reference
        {
            unsigned int len = ctrl >> 5;
            if (len == 7)
            {
                if (ip >= ip_end) return -1;
                len += *ip++;
            }
            len += 2;

            if (ip >= ip_end) return -1;
            unsigned int offset = ((ctrl & 0x1f) << 8) + *ip++ + 1;
            unsigned char* ref = op - offset;
            if (ref < reinterpret_cast<unsigned char*>(out)) return -1;
            if (op + len > op_end) return -1;
            for (unsigned int i = 0; i < len; ++i)
                *op++ = ref[i];
        }
    }
    return static_cast<int>(op - reinterpret_cast<unsigned char*>(out));
}

std::optional<PCData> PointCloudHandler::buildFromPCD(const QString& filePath)
{
    spdlog::trace("[General] PointCloudHandler::buildFromPCD: {}", filePath.toStdString());

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        spdlog::error("[General] Failed to open file: {}", filePath.toStdString());
        return std::nullopt;
    }

    // --- Parse header ---
    int pcdWidth = 0, pcdHeight = 0, pcdPoints = 0;
    int xIdx = -1, yIdx = -1, zIdx = -1;
    int fieldCount = 0;
    std::vector<int> fieldSizes;
    std::string dataType;

    while (!file.atEnd())
    {
        const QString line = QString::fromLatin1(file.readLine()).trimmed();
        if (line.startsWith('#')) continue;

        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;

        const QString key = parts[0].toUpper();

        if (key == "FIELDS")
        {
            for (int i = 1; i < parts.size(); ++i)
            {
                const QString f = parts[i].toLower();
                if      (f == "x") xIdx = i - 1;
                else if (f == "y") yIdx = i - 1;
                else if (f == "z") zIdx = i - 1;
            }
            fieldCount = parts.size() - 1;
        }
        else if (key == "SIZE")
        {
            for (int i = 1; i < parts.size(); ++i)
                fieldSizes.push_back(parts[i].toInt());
        }
        else if (key == "WIDTH")  pcdWidth  = parts.value(1).toInt();
        else if (key == "HEIGHT") pcdHeight = parts.value(1).toInt();
        else if (key == "POINTS") pcdPoints = parts.value(1).toInt();
        else if (key == "DATA")
        {
            dataType = parts.value(1).toLower().toStdString();
            break;
        }
    }

    if (xIdx < 0 || yIdx < 0 || zIdx < 0)
    {
        spdlog::error("[General] PCD missing x/y/z fields: {}", filePath.toStdString());
        return std::nullopt;
    }
    if (pcdPoints <= 0)
    {
        spdlog::error("[General] PCD has no points: {}", filePath.toStdString());
        return std::nullopt;
    }
    if (fieldSizes.empty())
        fieldSizes.assign(fieldCount, 4);

    // Compute per-field byte offsets
    int bytesPerPoint = 0;
    std::vector<int> fieldOffsets(fieldSizes.size());
    for (int i = 0; i < static_cast<int>(fieldSizes.size()); ++i)
    {
        fieldOffsets[i] = bytesPerPoint;
        bytesPerPoint += fieldSizes[i];
    }
    const int offX = fieldOffsets[xIdx];
    const int offY = fieldOffsets[yIdx];
    const int offZ = fieldOffsets[zIdx];

    const float kNaN = std::numeric_limits<float>::quiet_NaN();
    const bool isOrganized = (pcdHeight > 1);
    const int matW = isOrganized ? pcdWidth  : pcdPoints;
    const int matH = isOrganized ? pcdHeight : 1;

    cv::Mat matPC(matH, matW, CV_32FC3, cv::Scalar(kNaN, kNaN, kNaN));
    float maxZ = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    int pointCount = 0;

    auto storePoint = [&](int row, int col, const char* buf)
    {
        float x, y, z;
        memcpy(&x, buf + offX, sizeof(float));
        memcpy(&y, buf + offY, sizeof(float));
        memcpy(&z, buf + offZ, sizeof(float));
        if (std::isfinite(x) && std::isfinite(y) && std::isfinite(z)
            && (x != 0.f || y != 0.f || z != 0.f))
        {
            matPC.at<cv::Vec3f>(row, col) = { x, y, z };
            maxZ = std::max(maxZ, z);
            minZ = std::min(minZ, z);
            ++pointCount;
        }
    };

    if (dataType == "ascii")
    {
        std::vector<char> buf(bytesPerPoint, 0);
        for (int i = 0; i < pcdPoints && !file.atEnd(); ++i)
        {
            const QStringList parts = QString::fromLatin1(file.readLine())
                                          .trimmed()
                                          .split(' ', Qt::SkipEmptyParts);
            int off = 0;
            for (int f = 0; f < static_cast<int>(fieldSizes.size()) && f < parts.size(); ++f)
            {
                if (fieldSizes[f] == 4)
                {
                    float v = parts[f].toFloat();
                    memcpy(buf.data() + off, &v, sizeof(float));
                }
                off += fieldSizes[f];
            }
            const int row = isOrganized ? (i / pcdWidth) : 0;
            const int col = isOrganized ? (i % pcdWidth) : i;
            storePoint(row, col, buf.data());
        }
    }
    else if (dataType == "binary")
    {
        const QByteArray data = file.readAll();
        for (int i = 0; i < pcdPoints; ++i)
        {
            const int byteOff = i * bytesPerPoint;
            if (byteOff + bytesPerPoint > data.size()) break;
            const int row = isOrganized ? (i / pcdWidth) : 0;
            const int col = isOrganized ? (i % pcdWidth) : i;
            storePoint(row, col, data.constData() + byteOff);
        }
    }
    else if (dataType == "binary_compressed")
    {
        // PCL binary_compressed format:
        // [uint32 compressed_size][uint32 uncompressed_size][LZF data]
        // Decompressed layout is SOA: all field0 values, then all field1 values, ...
        uint32_t compSz = 0, rawSz = 0;
        if (file.read(reinterpret_cast<char*>(&compSz), 4) != 4 ||
            file.read(reinterpret_cast<char*>(&rawSz),  4) != 4)
        {
            spdlog::error("[General] PCD binary_compressed header truncated: {}", filePath.toStdString());
            return std::nullopt;
        }

        const QByteArray compressed = file.read(compSz);
        if (static_cast<uint32_t>(compressed.size()) != compSz)
        {
            spdlog::error("[General] PCD compressed data truncated: {}", filePath.toStdString());
            return std::nullopt;
        }

        QByteArray decompressed(static_cast<int>(rawSz), '\0');
        const int written = lzfDecompress(compressed.constData(), static_cast<int>(compSz),
                                          decompressed.data(), static_cast<int>(rawSz));
        if (written < 0 || static_cast<uint32_t>(written) != rawSz)
        {
            spdlog::error("[General] PCD LZF decompression failed: {}", filePath.toStdString());
            return std::nullopt;
        }

        // SOA layout: field f occupies [fieldStarts[f], fieldStarts[f] + fieldSizes[f]*pcdPoints)
        std::vector<int> fieldStarts(fieldSizes.size());
        int soaOff = 0;
        for (int f = 0; f < static_cast<int>(fieldSizes.size()); ++f)
        {
            fieldStarts[f] = soaOff;
            soaOff += fieldSizes[f] * pcdPoints;
        }

        const char* buf = decompressed.constData();
        for (int i = 0; i < pcdPoints; ++i)
        {
            float x, y, z;
            memcpy(&x, buf + fieldStarts[xIdx] + i * fieldSizes[xIdx], sizeof(float));
            memcpy(&y, buf + fieldStarts[yIdx] + i * fieldSizes[yIdx], sizeof(float));
            memcpy(&z, buf + fieldStarts[zIdx] + i * fieldSizes[zIdx], sizeof(float));
            if (std::isfinite(x) && std::isfinite(y) && std::isfinite(z)
                && (x != 0.f || y != 0.f || z != 0.f))
            {
                const int row = isOrganized ? (i / pcdWidth) : 0;
                const int col = isOrganized ? (i % pcdWidth) : i;
                matPC.at<cv::Vec3f>(row, col) = { x, y, z };
                maxZ = std::max(maxZ, z);
                minZ = std::min(minZ, z);
                ++pointCount;
            }
        }
    }
    else
    {
        spdlog::error("[General] PCD data type '{}' not supported: {}", dataType, filePath.toStdString());
        return std::nullopt;
    }

    file.close();
    spdlog::info("[General] PCD loaded: {} points, MaxZ: {:.2f}, MinZ: {:.2f}", pointCount, maxZ, minZ);
    return PCData{ matPC, maxZ, minZ, pointCount };
}

std::optional<PCData> PointCloudHandler::fromCropped(const CroppedData& cd)
{
    float maxZ = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    int pointCount = 0;

    for (int r = 0; r < cd.pointCloud.rows; ++r)
    {
        const cv::Vec3f* row = cd.pointCloud.ptr<cv::Vec3f>(r);
        for (int c = 0; c < cd.pointCloud.cols; ++c)
        {
            if (isValidPoint(row[c]))
            {
                ++pointCount;
                minZ = std::min(minZ, row[c][2]);
                maxZ = std::max(maxZ, row[c][2]);
            }
        }
    }

    if (pointCount == 0)
        return std::nullopt;

    return PCData{ cd.pointCloud, maxZ, minZ, pointCount };
}