/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Clom Huang   huangcailong@kylinos.cn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UTILS_GPU_DETECTION
#define UTILS_GPU_DETECTION
#include "gpu_types.h"

class GpuDetection {

public:    
    static GpuType getGpuModel();

private:
    static bool isJjwGraphicCard();
    static bool isGP101GraphicCard();
    static bool isAMDGraphicCard();
    static bool isNvidiaGraphicCard();
    static bool isMaliGraphicCard();
    static bool mGpuCheckCompleted;
    static GpuType mGpuType;
};

#endif
