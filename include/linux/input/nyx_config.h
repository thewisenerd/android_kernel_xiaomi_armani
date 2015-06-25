/*
 * include/linux/input/nyx_config.h
 *
 * Copyright (c) 2015, Vineeth Raj <thewisenerd@protonmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _LINUX_PROJECT_NYX_H
#define _LINUX_PROJECT_NYX_H

/* SIZEOF_COORD defines the size required to store one set of co-ordinates */
#define NYX_SIZEOF_COORD   3

/* SIZEOF_CHARBUF defines the total number of co-ordinates we shalt store */
#define NYX_SIZEOF_CHARBUF 512

/* SIZEOF_HEADER_BUF defines the total number of bytes required to store
 * the total number of points logged
 */
#define NYX_SIZEOF_HEADER_BUF 2

/* total = SIZEOF_HEADER_BUF + (SIZEOF_COORD * SIZEOF_CHARBUF)
 * check if total > PAGE_SIZE
 * if total > PAGESIZE, data should be split into pages
 */
#if ((NYX_SIZEOF_HEADER_BUF + (NYX_SIZEOF_COORD * NYX_SIZEOF_CHARBUF)) > PAGE_SIZE)

/* SIZEOF_PAGENEXT defines the number of bytes required to store the variable
 * necessary to check if there exists another page of data
 */
#define NYX_SIZEOF_PAGENEXT 1
#define NYX_ISDEF_PAGENEXT

#endif // total > PAGE_SIZE

#define NYX_TOTAL_SIZEOF \
	NYX_SIZEOF_HEADER_BUF + \
	(NYX_SIZEOF_COORD * NYX_SIZEOF_CHARBUF)

#ifdef NYX_ISDEF_PAGENEXT
#define NYX_TOTAL_SIZEOF NYX_TOTAL_SIZEOF + NYX_SIZEOF_PAGENEXT
#endif

#ifdef NYX_DEBUG
#if NYX_DEBUG == 1
#ifdef NYX_VERB_LVL
#if NYX_VERB_LVL == 1
#define NYX_DBG_LVL1
#elif NYX_VERB_LVL == 2
#define NYX_DBG_LVL1
#define NYX_DBG_LVL2
#elif NYX_VERB_LVL == 3
#define NYX_DBG_LVL1
#define NYX_DBG_LVL2
#define NYX_DBG_LVL3
#else
#define NYX_DBG_LVL2
#endif // else
#endif // ifdef NYX_VERB_LVL
#endif // if nyx_debug
#endif // ifdef nyx_debug

#define ONEIROI_NOTIFY "/sys/nyx/nyx_gesture"

/* -------------------------------------------------------------------------- */

extern int nyx_switch;

#endif /* _LINUX_PROJECT_NYX_H */
