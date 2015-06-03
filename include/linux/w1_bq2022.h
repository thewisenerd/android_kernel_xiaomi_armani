/*
 * include/linux/w1_bq2022.h
 *
 * Copyright (C) 2015 Balázs Triszka <balika011@protonmail.ch>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#ifndef _LINUX_W1_BQ2022_H
#define _LINUX_W1_BQ2022_H

int w1_bq2022_has_battery_data(void);
int w1_bq2022_get_battery_id(void);

#endif /* _LINUX_W1_BQ2022_H */
