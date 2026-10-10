#pragma once

#include "Types.h"
#include <optional>
#include <QString>

class PointCloudHandler
{
public:
	PointCloudHandler() = delete;

	static std::optional<PCData> buildFromPCD(const QString& filePath);
	static std::optional<PCData> fromCropped(const CroppedData& cd);
};