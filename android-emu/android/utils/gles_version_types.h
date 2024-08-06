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

#ifndef _GLES_VERSION_TYPES
#define _GLES_VERSION_TYPES

typedef enum GLESVersionType {
    GLESVersion_unknown = 0,
    GLESVersion_1_0,
    GLESVersion_2_0,
    GLESVersion_3_0,
    GLESVersion_3_1,
} GLESVersionType;

typedef enum FeatureStatus {
    Status_unknown = 0,
    Status_disable,
    Status_enable,
} FeatureStatus;

#endif
