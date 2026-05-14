#pragma once
#include <string>
#include "Chart.h"

namespace ChartIO
{
    // Chart を JSON ファイルに保存。成功したら true を返す
    bool save(const Chart& chart, const std::string& filePath);

    // JSON ファイルから Chart を読み込む。成功したら true を返す
    bool load(const std::string& filePath, Chart& out);
}
