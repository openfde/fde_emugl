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

#ifndef _GPU_TYPES
#define _GPU_TYPES

typedef enum GpuType {    
    UNKNOWN_VGA = -1,
    NVIDIA_VGA = 0,
    AMD_VGA = 1,
    MALI_VGA = 2,
    INTEL_VGA = 3,
    GP101_VGA = 4,
    ZC716_VGA = 5,
    JJM_VGA = 6,
    VIRTUAL_VGA = 7,
    ZHAOXIN_VGA = 8,
    OTHER_VGA = 9
} GpuType;   

#endif
