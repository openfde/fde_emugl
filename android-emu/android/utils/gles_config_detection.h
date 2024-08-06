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

#ifndef GLES_CONFIG_DETECTION
#define GLES_CONFIG_DETECTION
#include "gles_version_types.h"

class GLESConfigDetection {

public:    
    static GLESVersionType getGLESVersion();
    static FeatureStatus getEGLStatus();
    static FeatureStatus getNTDCStatus();
    //static bool isEgl2Egl();
    static bool isGLESConfigExist();
    static bool isEnableASTC();

private:    
    static void readRenderGLESConfig();
    static GLESVersionType mGLESVersionType;
    static FeatureStatus mEGLStatus;
    static FeatureStatus mNTDCStatus;
    //static bool mEgl2Egl;
    static bool mGLESConfigExist;
    static bool mEnableASTC;
};

#endif
